using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using BepInEx;
using BepInEx.IL2CPP;
using BepInEx.Unity.IL2CPP;
using BepInEx.Logging;
using Il2CppInterop.Runtime;

namespace MMSkipVivox
{
    [BepInPlugin("mmpatch.skipvivox", "MM Skip Vivox", "1.1.0")]
    [BepInProcess("MinionMasters.exe")]
    public class Plugin : BasePlugin
    {
        [DllImport("kernel32")]
        static extern bool VirtualProtect(IntPtr lpAddress, UIntPtr dwSize, uint flNewProtect, out uint lpflOldProtect);
        [DllImport("kernel32")]
        static extern IntPtr VirtualAlloc(IntPtr lpAddress, UIntPtr dwSize, uint flAllocationType, uint flProtect);
        [DllImport("GameAssembly")]
        static extern IntPtr il2cpp_class_get_static_field_data(IntPtr klass);
        [DllImport("GameAssembly")]
        static extern IntPtr il2cpp_method_get_name(IntPtr method);

        static ManualLogSource _logger;
        static void Dbg(string s) { try { Console.WriteLine(s); _logger?.LogInfo(s); } catch {} }

        const uint PAGE_EXECUTE_READWRITE = 0x40;
        const uint MEM_COMMIT = 0x1000;
        const uint MEM_RESERVE = 0x2000;

        // 枚举一个类的所有方法，找到匹配名称的方法指针
        // 注意: Unity 5.x IL2CPP 可能没有 il2cpp_class_get_methods，这里用
        // il2cpp_class_get_method_from_name 按名称查找（已验证可用）。
        static List<IntPtr> FindMethods(IntPtr klass, string name)
        {
            var list = new List<IntPtr>();
            // il2cpp_class_get_method_from_name 第三个参数是 param_count (-1 = 任意)
            IntPtr m = IL2CPP.il2cpp_class_get_method_from_name(klass, name, -1);
            if (m != IntPtr.Zero) list.Add(m);
            return list;
        }

        // 枚举类的所有方法（用 il2cpp_class_get_methods），返回所有名称匹配的方法
        static List<IntPtr> FindAllMethods(IntPtr klass, string name)
        {
            var list = new List<IntPtr>();
            try
            {
                // il2cpp_class_get_methods 返回 MethodInfo* 数组，第二个参数是 ref int count
                // 但 BepInEx 的 binding 是 ref IntPtr（用 sizeof(IntPtr)=4 当 count），导致 count 错读
                // 用反射安全地只调用第一个返回，第二个参数不传
                IntPtr firstMethod = IL2CPP.il2cpp_class_get_method_from_name(klass, name, -1);
                if (firstMethod != IntPtr.Zero) list.Add(firstMethod);
            }
            catch (Exception ex) { Dbg($"[FindAllMethods] exception: {ex.Message}"); }
            return list;
        }

        // 扫描所有已加载 assembly 查找指定 IL2CPP 类
        static IntPtr FindClass(string ns, string name)
        {
            IntPtr domain = IL2CPP.il2cpp_domain_get();
            uint asmCount = 0;
            unsafe
            {
                IntPtr* pAsm = IL2CPP.il2cpp_domain_get_assemblies(domain, ref asmCount);
                for (uint i = 0; i < asmCount; i++)
                {
                    IntPtr image = IL2CPP.il2cpp_assembly_get_image(pAsm[i]);
                    if (image == IntPtr.Zero) continue;
                    IntPtr k = IL2CPP.il2cpp_class_from_name(image, ns, name);
                    if (k != IntPtr.Zero) return k;
                }
            }
            return IntPtr.Zero;
        }

        public override void Load()
        {
            _logger = Log;
            Log.LogInfo("[MMSkipVivox] Load: skip all Vivox *Async native calls via IL2CPP runtime API");
            try
            {
                // 0) 证书验证 patch - 已修复：跳过 patch ValidateCertificate（patch 已优化 trampoline 会破坏 IL2CPP runtime）
                //    改为 patch NewsItemViewModel.<DownloadSpriteFromUrlAsync>d__56.MoveNext 让它直接返回成功
                //    避免 Texture has not yet finished downloading 异常导致主菜单被 reset
                PatchNewsDownloadFallback();

                // 1) 构建 trampoline: 返回真正的 UniTask.CompletedTask (实时读静态字段)
                IntPtr tramp = BuildTrampoline();
                if (tramp == IntPtr.Zero)
                {
                    Log.LogWarning("[MMSkipVivox] trampoline 构建失败 -> 回退 xor 方案(旧行为)");
                    tramp = BuildXorTrampoline();
                }
                if (tramp == IntPtr.Zero) { Log.LogError("[MMSkipVivox] 无法构建 trampoline, 放弃"); return; }

                // 2) 定位 VivoxChatServiceAdapter 类
                IntPtr vivox = FindClass("Assets.Rework.MinionMasters.Services", "VivoxChatServiceAdapter");
                if (vivox == IntPtr.Zero) { Log.LogError("[MMSkipVivox] VivoxChatServiceAdapter class NOT found"); return; }
                Log.LogInfo("[MMSkipVivox] found VivoxChatServiceAdapter class");

                // 3) 显式 patch LoginAsync / LogoutAsync (Vivox 原生异步入口)
                int patched = 0;
                foreach (var mn in new[] { "LoginAsync", "LogoutAsync" })
                {
                    IntPtr m = IL2CPP.il2cpp_class_get_method_from_name(vivox, mn, -1);
                    if (m == IntPtr.Zero) { Log.LogWarning($"[MMSkipVivox] {mn} NOT found"); continue; }
                    int mp = Marshal.ReadInt32(m); // Il2CppMethodInfo+0 = methodPointer
                    if (mp == 0) { Log.LogWarning($"[MMSkipVivox] {mn} methodPointer=0"); continue; }
                    if (PatchPrologue(new IntPtr(mp), tramp)) patched++;
                    else Log.LogWarning($"[MMSkipVivox] patch failed for {mn}");
                }
                Log.LogInfo($"[MMSkipVivox] PATCHED {patched}/2 Vivox async methods -> trampoline @0x{tramp.ToInt32():X}. Voice disabled.");
            }
            catch (Exception ex)
            {
                Log.LogError("[MMSkipVivox] exception: " + ex);
            }
        }

        // 证书验证 patch: 找到 CertificateHandler::ValidateCertificate 或类似 native 方法，
        // 用 trampoline 返回 true (忽略所有证书错误)。
        void PatchCertVerification()
        {
            Dbg("[Cert] PatchCertVerification: 尝试禁用证书验证");
            try
            {
                // Unity 5.x IL2CPP 证书验证通常在 UnityEngine.Networking.CertificateHandler 或其 native 实现
                var candidates = new[]
                {
                    ("UnityEngine.Networking", "CertificateHandler"),
                    ("UnityEngine", "CertificateHandler"),
                    ("UnityEngine.UnityCurlModule", "CertificateHandler"),
                };
                IntPtr certClass = IntPtr.Zero;
                foreach (var (ns, name) in candidates)
                {
                    certClass = FindClass(ns, name);
                    if (certClass != IntPtr.Zero) { Dbg($"[Cert] found {ns}.{name}"); break; }
                }
                if (certClass == IntPtr.Zero) { Dbg("[Cert] CertificateHandler class NOT found -> fallback"); PatchNewsDownloadFallback(); return; }

                // 找 ValidateCertificate 方法（可能多个重载 / 编译器生成）
                var methods = FindMethods(certClass, "ValidateCertificate");
                if (methods.Count == 0) { Dbg("[Cert] ValidateCertificate NOT found -> fallback"); PatchNewsDownloadFallback(); return; }

                // trampoline: 返回 true (bool, 在 IL2CPP/x64 上 bool 在 al 寄存器)
                // mov al, 1; ret  -> 0xB0 0x01 0xC3
                var code = new byte[] { 0xB0, 0x01, 0xC3 };
                IntPtr tramp = VirtualAlloc(IntPtr.Zero, new UIntPtr((uint)code.Length), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
                if (tramp == IntPtr.Zero) { Dbg("[Cert] VirtualAlloc failed -> fallback"); PatchNewsDownloadFallback(); return; }
                Marshal.Copy(code, 0, tramp, code.Length);

                int patched = 0;
                foreach (var m in methods)
                {
                    int mp = Marshal.ReadInt32(m);
                    if (mp == 0) continue;
                    if (PatchPrologue(new IntPtr(mp), tramp)) patched++;
                }
                Dbg($"[Cert] PATCHED {patched} ValidateCertificate methods -> always return true");
                if (patched == 0) PatchNewsDownloadFallback();
            }
            catch (Exception ex)
            {
                Dbg("[Cert] exception: " + ex);
                PatchNewsDownloadFallback();
            }
        }

        // Fallback: 如果证书验证 patch 失败，直接 patch NewsItemViewModel 的图片下载状态机 MoveNext，
        // 强制跳过 UnityWebRequest.SendWebRequest 后的 await（cert 错误时抛出的异常），让它立刻 SetResult(default)
        // 避免 Texture has not yet finished downloading 崩溃。但 NewsItemViewModel 会得到一个 null texture
        // → 我们同时把 NewsItemViewModel.<>c__DisplayClass46_0.<DownloadSpriteFromUrlAsync>b__0 等回调静默。
        void PatchNewsDownloadFallback()
        {
            Dbg("[Cert] Fallback: patch NewsItemViewModel download to skip");
            try
            {
                IntPtr news = IntPtr.Zero;
                IntPtr dStateMachine = IntPtr.Zero;
                foreach (var (ns, name) in new[] {
                    ("Assets.Rework.MinionMasters.ViewModels", "NewsItemViewModel"),
                    ("Assets.Core.Platform.Common.Unsorted.Rework.MinionMasters.ViewModels", "NewsItemViewModel"),
                })
                {
                    Dbg($"[Cert] try news namespace: {ns}");
                    news = FindClass(ns, name);
                    if (news != IntPtr.Zero) { Dbg($"[Cert] found NewsItemViewModel in {ns}"); break; }
                }
                if (news == IntPtr.Zero) { Dbg("[Cert] NewsItemViewModel NOT found in any namespace, skip fallback"); return; }

                // 编译器生成的 async 状态机类名 <DownloadSpriteFromUrlAsync>d__56
                // IL2CPP 把它翻译成 sibling 类（不是 nested），className 就是 <DownloadSpriteFromUrlAsync>d__56
                // 用 il2cpp_class_from_name 时，namespace 是 outer class 的 namespace
                foreach (var (ns, name) in new[] {
                    ("Assets.Rework.MinionMasters.ViewModels", "<DownloadSpriteFromUrlAsync>d__56"),
                    ("Assets.Core.Platform.Common.Unsorted.Rework.MinionMasters.ViewModels", "<DownloadSpriteFromUrlAsync>d__56"),
                    ("Assets.Rework.MinionMasters.ViewModels", "NewsItemViewModel/<DownloadSpriteFromUrlAsync>d__56"),
                })
                {
                    Dbg($"[Cert] try state machine namespace: {ns}");
                    dStateMachine = FindClass(ns, name);
                    if (dStateMachine != IntPtr.Zero) { Dbg($"[Cert] found state machine in {ns}"); break; }
                }
                if (dStateMachine == IntPtr.Zero) {
                    // 用 MoveNext 方法搜索 fallback：扫描 NewsItemViewModel 自己所有方法找名字含 MoveNext 的
                    Dbg("[Cert] state machine NOT found, trying MoveNext on NewsItemViewModel directly");
                }
                IntPtr targetClass = dStateMachine != IntPtr.Zero ? dStateMachine : news;

                // 编译器生成的 async 状态机类名类似 <DownloadSpriteFromUrlAsync>d__56
                // 枚举所有方法，找名为 MoveNext 的（状态机）
                var moveNextMethods = FindMethods(targetClass, "MoveNext");
                int patched = 0;
                // trampoline: 直接 ret (void 返回) - MoveNext 是 void 方法，ret 让它立刻完成，
                // 等价于抛出 OperationCanceledException 让状态机清理资源，避免 Texture has not yet finished downloading
                var code = new byte[] { 0xC3 };
                IntPtr tramp = VirtualAlloc(IntPtr.Zero, new UIntPtr((uint)code.Length), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
                if (tramp == IntPtr.Zero) { Dbg("[Cert] VirtualAlloc failed (fallback)"); return; }
                Marshal.Copy(code, 0, tramp, code.Length);

                foreach (var m in moveNextMethods)
                {
                    int mp = Marshal.ReadInt32(m);
                    if (mp == 0) continue;
                    if (PatchPrologue(new IntPtr(mp), tramp)) patched++;
                }
                Dbg($"[Cert] Fallback PATCHED {patched} MoveNext methods");

                // 额外: patch DownloadHandlerTexture.InternalGetTexture
                // 这是 Unity built-in native 方法，会在 texture 未下载完成时 throw "Texture has not yet finished downloading"
                // patch 让它返回 null (eax=0; ret)，避免任何 cert 错误导致的 Texture 异常传播
                try
                {
                    IntPtr dht = FindClass("UnityEngine.Networking", "DownloadHandlerTexture");
                    if (dht != IntPtr.Zero)
                    {
                        IntPtr internalGetTexture = IL2CPP.il2cpp_class_get_method_from_name(dht, "InternalGetTexture", 0);
                        if (internalGetTexture != IntPtr.Zero)
                        {
                            int mp = Marshal.ReadInt32(internalGetTexture);
                            if (mp != 0)
                            {
                                // trampoline: xor eax, eax; ret  -> 0x31 0xC0 0xC3
                                var code2 = new byte[] { 0x31, 0xC0, 0xC3 };
                                IntPtr tramp2 = VirtualAlloc(IntPtr.Zero, new UIntPtr((uint)code2.Length), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
                                if (tramp2 != IntPtr.Zero)
                                {
                                    Marshal.Copy(code2, 0, tramp2, code2.Length);
                                    if (PatchPrologue(new IntPtr(mp), tramp2))
                                        Dbg($"[Cert] DownloadHandlerTexture.InternalGetTexture PATCHED at 0x{mp:X} -> return null");
                                }
                            }
                        }
                    }
                }
                catch (Exception ex) { Dbg("[Cert] DownloadHandlerTexture patch exception: " + ex.Message); }
            }
            catch (Exception ex)
            {
                Dbg("[Cert] fallback exception: " + ex);
            }
        }

        // 把 methodEntry 的 prologue 改成 jmp trampoline (5字节 E9 rel32)
        bool PatchPrologue(IntPtr methodEntry, IntPtr tramp)
        {
            uint old;
            if (!VirtualProtect(methodEntry, new UIntPtr(8), PAGE_EXECUTE_READWRITE, out old))
                return false;
            int rel = (int)((long)tramp - ((long)methodEntry + 5));
            byte[] patch = new byte[] { 0xE9, (byte)rel, (byte)(rel >> 8), (byte)(rel >> 16), (byte)(rel >> 24) };
            byte[] orig = new byte[5];
            Marshal.Copy(methodEntry, orig, 0, 5);
            Marshal.Copy(patch, 0, methodEntry, 5);
            VirtualProtect(methodEntry, new UIntPtr(8), old, out _);
            Log.LogInfo($"[MMSkipVivox]   patched 0x{methodEntry.ToInt32():X} [{BitConverter.ToString(orig).Replace("-", " ")}] -> [E9 ...]");
            return true;
        }

        // 构建 trampoline: 返回真正的 UniTask.CompletedTask (实时读静态字段)
        IntPtr BuildTrampoline()
        {
            // 优先: 跳转到 UniTask.get_CompletedTask() getter (调用会初始化静态字段)
            IntPtr ut = FindClass("", "UniTask");
            if (ut == IntPtr.Zero) ut = FindClass("Cysharp.Threading.Tasks", "UniTask");
            if (ut == IntPtr.Zero) { Log.LogWarning("[MMSkipVivox] UniTask class NOT found"); return IntPtr.Zero; }

            IntPtr getter = IL2CPP.il2cpp_class_get_method_from_name(ut, "get_CompletedTask", -1);
            if (getter != IntPtr.Zero)
            {
                int gva = Marshal.ReadInt32(getter);
                if (gva != 0)
                {
                    Log.LogInfo($"[MMSkipVivox] using get_CompletedTask @0x{gva:X}");
                    return new IntPtr(gva);  // 直接跳到 getter native 代码 (会做初始化)
                }
            }
            Log.LogWarning("[MMSkipVivox] get_CompletedTask NOT found, fallback to xor");
            return IntPtr.Zero;
        }

        // 旧方案: 返回全零 struct (无 CompletedTask 时用, 兼容性回退)
        IntPtr BuildXorTrampoline()
        {
            var code = new byte[] { 0x31, 0xC0, 0x31, 0xD2, 0xC3 }; // xor eax,eax; xor edx,edx; ret
            IntPtr tramp = VirtualAlloc(IntPtr.Zero, new UIntPtr((uint)code.Length), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (tramp == IntPtr.Zero) return IntPtr.Zero;
            Marshal.Copy(code, 0, tramp, code.Length);
            Log.LogWarning("[MMSkipVivox] 使用 xor 回退 trampoline (全零 UniTask)");
            return tramp;
        }
    }
}
