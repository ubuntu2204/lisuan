using System;
using System.Reflection;
using System.Linq;
var path="/home/ubuntu/.local/share/Steam/steamapps/common/Minion Masters/BepInEx/interop/Core.Platform.Common.dll";
try {
  var asm=Assembly.LoadFrom(path);
  var types=asm.GetTypes().Where(t=>(t.Name??"").Contains("Vivox")).ToArray();
  Console.WriteLine($"Found {types.Length} Vivox types in {asm.GetName().Name}:");
  foreach(var t in types.Take(30)) Console.WriteLine($"  FULL={t.FullName}  NAME={t.Name}  NS={(t.Namespace??"<null>")}");
  // also list types whose namespace contains MinionMasters.Services
  var svc=asm.GetTypes().Where(t=>(t.Namespace??"").Contains("Services")).Take(20).ToArray();
  Console.WriteLine($"Services types sample:");
  foreach(var t in svc.Take(20)) Console.WriteLine($"  {t.FullName}");
} catch(Exception e){ Console.WriteLine("ERR "+e); }
