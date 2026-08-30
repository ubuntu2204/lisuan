using System;using System.Reflection;using System.Linq;using System.IO;
class P{static void Main(){
var dir="/home/ubuntu/.local/share/Steam/steamapps/common/Minion Masters/BepInEx/interop/";
var alc=new System.Runtime.Loader.AssemblyLoadContext("tmp",true);
foreach(var f in Directory.GetFiles(dir,"*.dll")){
  Assembly asm; try{asm=alc.LoadFromAssemblyPath(f);}catch{continue;}
  Type[] types; try{types=asm.GetTypes();}catch(ReflectionTypeLoadException ex){types=ex.Types.Where(t=>t!=null).ToArray();}
  foreach(var t in types){
    if(t.FullName!=null && t.FullName.Contains("MinionMasters.Services"))
      Console.WriteLine($"[{Path.GetFileName(f)}] {t.FullName}");
  }
}
Console.WriteLine("DONE");
}}
