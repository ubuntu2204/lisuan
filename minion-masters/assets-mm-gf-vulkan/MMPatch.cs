// MMPatch: doorstop 直载的 IL2CPP patch（不依赖 BepInEx Chainloader / Il2CppInterop）
//
// 【权威版本】本文件是唯一有效源码，构建产物 bin/MMPatch.dll 由部署脚本取用。
// 历史副本 mm-patch/doorstop/MMPatch.cs（2026-08-26，Vivox patch 仍启用，部署后
// 游戏会卡在登录）已随 mm-patch/ 目录于 2026-08-31 删除；git 历史可查，勿再还原。
// 详见同目录 README.md。
//
// 三个 patch（与原 MMSkipVivox 插件等效，该 BepInEx 插件已退役）：
//  a. VivoxChatServiceAdapter.LoginAsync/LogoutAsync -> jmp xor trampoline（全零 UniTask）
//     【已停用】Vivox 在 wine 下原生可用，patch 反而卡登录，见下方 Run() 内注释
//  b. NewsItemViewModel 下载状态机 MoveNext -> ret
//  c. DownloadHandlerTexture.InternalGetTexture -> xor eax,eax; ret（返回 null）
using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.IO;

namespace Doorstop
{
    public static class Entrypoint
    {
        public static void Start()
        {
            try
            {
                var t = new Thread(MMPatch.Run);
                t.IsBackground = true;
                t.Start();
            }
            catch (Exception ex) { MMPatch.Log("Start exception: " + ex); }
        }
    }
}

public static class MMPatch
{
    const string LogPath = @"BepInEx\MMPatch.log";
    public static void Log(string s)
    {
        try { File.AppendAllText(LogPath, $"[{DateTime.Now:HH:mm:ss}] {s}\n"); } catch { }
        try { Console.WriteLine(s); } catch { }
    }

    // ---- kernel32 ----
    [DllImport("kernel32")] static extern bool VirtualProtect(IntPtr addr, UIntPtr size, uint newProt, out uint oldProt);
    [DllImport("kernel32")] static extern IntPtr VirtualAlloc(IntPtr addr, UIntPtr size, uint flAlloc, uint flProt);
    [DllImport("kernel32")] static extern IntPtr GetModuleHandle(string name);

    // ---- IL2CPP C API（GameAssembly.dll 导出）----
    [DllImport("GameAssembly")] static extern IntPtr il2cpp_domain_get();
    [DllImport("GameAssembly")] static extern IntPtr il2cpp_assembly_get_image(IntPtr asm_);
    [DllImport("GameAssembly")] static extern IntPtr il2cpp_class_from_name(IntPtr image, string ns, string name);
    [DllImport("GameAssembly")] static extern IntPtr il2cpp_class_get_method_from_name(IntPtr klass, string name, int paramCount);
    // il2cpp_domain_get_assemblies(domain, ref uint size) -> IntPtr* (unsafe 遍历)
    [DllImport("GameAssembly")] static extern unsafe IntPtr* il2cpp_domain_get_assemblies(IntPtr domain, ref uint size);

    const uint PAGE_RWX = 0x40, MEM_COMMIT = 0x1000, MEM_RESERVE = 0x2000;

    public static void Run()
    {
        Log("[MMPatch] waiting for IL2CPP domain...");
        IntPtr domain = IntPtr.Zero;
        for (int i = 0; i < 600; i++)  // 最多等 120s
        {
            try
            {
                domain = il2cpp_domain_get();
                if (domain != IntPtr.Zero) break;
            }
            catch { }
            Thread.Sleep(200);
        }
        if (domain == IntPtr.Zero) { Log("[MMPatch] il2cpp_domain_get timeout, abort"); return; }
        Log($"[MMPatch] domain ready @0x{domain.ToInt32():X}, applying patches");

        /*
         * Vivox patch【必须保持停用】（2026-08-29 再次确认）：
         * Vivox 在 wine 下本来就能正常工作——成功日志里有完整的
         *   [VivoxChatServiceAdapter] Initializing Vivox / Login started with token e30... /
         *   OnLoginStateChanged: LoggedIn
         * 一旦把 LoginAsync+LogoutAsync 改掉（无论改空操作还是跳 trampoline），
         * 游戏都会卡在 [ChatService] Starting LoginAsync 等不到回调
         * （表现为空操作版直接退出、trampoline 版卡在"加载中"）。
         * 另：日志中若看到 "PATCHED 2/2 Vivox async" 即说明本行没被注释掉，游戏必卡。
         */
        // try { PatchVivox(); } catch (Exception ex) { Log("[MMPatch] PatchVivox ex: " + ex); }
        Log("[MMPatch] Vivox patch disabled (Vivox works natively under wine)");

        try { PatchNewsDownloadFallback(); } catch (Exception ex) { Log("[MMPatch] PatchNews ex: " + ex); }
        Log("[MMPatch] done");
    }

    static IntPtr FindClass(string ns, string name)
    {
        IntPtr domain = il2cpp_domain_get();
        uint count = 0;
        unsafe
        {
            IntPtr* pAsm = il2cpp_domain_get_assemblies(domain, ref count);
            for (uint i = 0; i < count; i++)
            {
                IntPtr image = il2cpp_assembly_get_image(pAsm[i]);
                if (image == IntPtr.Zero) continue;
                IntPtr k = il2cpp_class_from_name(image, ns, name);
                if (k != IntPtr.Zero) return k;
            }
        }
        return IntPtr.Zero;
    }

    static IntPtr NewTrampoline(byte[] code)
    {
        IntPtr t = VirtualAlloc(IntPtr.Zero, new UIntPtr((uint)code.Length), MEM_COMMIT | MEM_RESERVE, PAGE_RWX);
        if (t != IntPtr.Zero) Marshal.Copy(code, 0, t, code.Length);
        return t;
    }

    static bool PatchPrologue(IntPtr entry, IntPtr tramp, string tag)
    {
        uint old;
        if (!VirtualProtect(entry, new UIntPtr(8), PAGE_RWX, out old)) { Log($"[MMPatch] {tag}: VirtualProtect failed @0x{entry.ToInt32():X}"); return false; }
        int rel = (int)((long)tramp - ((long)entry + 5));
        byte[] patch = { 0xE9, (byte)rel, (byte)(rel >> 8), (byte)(rel >> 16), (byte)(rel >> 24) };
        byte[] orig = new byte[5];
        Marshal.Copy(entry, orig, 0, 5);
        Marshal.Copy(patch, 0, entry, 5);
        VirtualProtect(entry, new UIntPtr(8), old, out _);
        Log($"[MMPatch]   patched {tag} 0x{entry.ToInt32():X} [{BitConverter.ToString(orig).Replace("-", " ")}] -> [E9 ...]");
        return true;
    }

    static void PatchVivox()
    {
        // xor eax,eax; xor edx,edx; ret  (全零 UniTask)
        IntPtr tramp = NewTrampoline(new byte[] { 0x31, 0xC0, 0x31, 0xD2, 0xC3 });
        if (tramp == IntPtr.Zero) { Log("[MMPatch] Vivox trampoline alloc failed"); return; }

        IntPtr vivox = FindClass("Assets.Rework.MinionMasters.Services", "VivoxChatServiceAdapter");
        if (vivox == IntPtr.Zero) { Log("[MMPatch] VivoxChatServiceAdapter NOT found"); return; }
        Log("[MMPatch] found VivoxChatServiceAdapter");

        int n = 0;
        foreach (var mn in new[] { "LoginAsync", "LogoutAsync" })
        {
            IntPtr m = il2cpp_class_get_method_from_name(vivox, mn, -1);
            if (m == IntPtr.Zero) { Log($"[MMPatch] {mn} NOT found"); continue; }
            int mp = Marshal.ReadInt32(m);
            if (mp == 0) continue;
            if (PatchPrologue(new IntPtr(mp), tramp, mn)) n++;
        }
        Log($"[MMPatch] PATCHED {n}/2 Vivox async -> trampoline @0x{tramp.ToInt32():X}");
    }

    static void PatchNewsDownloadFallback()
    {
        // b) NewsItemViewModel <DownloadSpriteFromUrlAsync>d__56.MoveNext -> ret
        IntPtr target = IntPtr.Zero;
        foreach (var (ns, name) in new[] {
            ("Assets.Rework.MinionMasters.ViewModels", "<DownloadSpriteFromUrlAsync>d__56"),
            ("Assets.Core.Platform.Common.Unsorted.Rework.MinionMasters.ViewModels", "<DownloadSpriteFromUrlAsync>d__56"),
        })
        {
            target = FindClass(ns, name);
            if (target != IntPtr.Zero) break;
        }
        if (target == IntPtr.Zero)
            target = FindClass("Assets.Rework.MinionMasters.ViewModels", "NewsItemViewModel");
        if (target != IntPtr.Zero)
        {
            IntPtr ret = NewTrampoline(new byte[] { 0xC3 });
            IntPtr m = il2cpp_class_get_method_from_name(target, "MoveNext", -1);
            if (ret != IntPtr.Zero && m != IntPtr.Zero)
            {
                int mp = Marshal.ReadInt32(m);
                if (mp != 0) PatchPrologue(new IntPtr(mp), ret, "MoveNext");
            }
        }
        else Log("[MMPatch] NewsItemViewModel state machine NOT found");

        // c) DownloadHandlerTexture.InternalGetTexture -> xor eax,eax; ret
        IntPtr dht = FindClass("UnityEngine.Networking", "DownloadHandlerTexture");
        if (dht != IntPtr.Zero)
        {
            IntPtr m = il2cpp_class_get_method_from_name(dht, "InternalGetTexture", 0);
            IntPtr tramp = NewTrampoline(new byte[] { 0x31, 0xC0, 0xC3 });
            if (tramp != IntPtr.Zero && m != IntPtr.Zero)
            {
                int mp = Marshal.ReadInt32(m);
                if (mp != 0) PatchPrologue(new IntPtr(mp), tramp, "InternalGetTexture->null");
            }
        }
        else Log("[MMPatch] DownloadHandlerTexture NOT found");
    }
}
