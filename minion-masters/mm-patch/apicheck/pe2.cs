using System;
using System.IO;
using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;
using System.Linq;
class P2 {
  static int Main(string[] a){
    using var fs=File.OpenRead(a[0]);
    using var pe=new PEReader(fs);
    var mr=pe.GetMetadataReader();
    foreach(var h in mr.TypeDefinitions){
      var td=mr.GetTypeDefinition(h);
      if(mr.GetString(td.Name)=="IL2CPP"){
        foreach(var mh in td.GetMethods()){
          var m=mr.GetMethodDefinition(mh);
          Console.WriteLine(mr.GetString(m.Name));
        }
      }
    }
    return 0;
  }
}
