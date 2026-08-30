using System;
using System.Collections.Immutable;
using System.IO;
using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;
class P {
  static int Main(string[] a){
    using var fs=File.OpenRead(a[0]);
    using var pe=new PEReader(fs);
    var mr=pe.GetMetadataReader();
    foreach(var h in mr.TypeDefinitions){
      var td=mr.GetTypeDefinition(h);
      if(mr.GetString(td.Name)!="IL2CPP") continue;
      foreach(var mh in td.GetMethods()){
        var m=mr.GetMethodDefinition(mh);
        var name=mr.GetString(m.Name);
        if(!(name.Contains("domain")||name.Contains("assembly")||name.Contains("image")||name=="il2cpp_class_from_name"||name=="il2cpp_class_get_method_from_name")) continue;
        var sig=m.DecodeSignature(new StrTypeProvider(), default);
        Console.WriteLine($"{name}({string.Join(", ", sig.ParameterTypes)}): {sig.ReturnType}");
      }
    }
    return 0;
  }
}
class StrTypeProvider : ISignatureTypeProvider<string, object> {
  public string GetArrayType(string elementType, ArrayShape shape)=>elementType+"[]";
  public string GetByReferenceType(string elementType)=>elementType+"&";
  public string GetFunctionPointerType(MethodSignature<string> signature)=>"fnptr";
  public string GetGenericInstantiation(string genericType, ImmutableArray<string> typeArguments)=>genericType;
  public string GetGenericMethodParameter(object genericContext, int index)=>"!!"+index;
  public string GetGenericTypeParameter(object genericContext, int index)=>"!"+index;
  public string GetModifiedType(string modifierType, string unmodifiedType, bool isRequired)=>unmodifiedType;
  public string GetPinnedType(string elementType)=>elementType;
  public string GetPointerType(string elementType)=>elementType+"*";
  public string GetPrimitiveType(PrimitiveTypeCode typeCode)=>typeCode.ToString();
  public string GetSZArrayType(string elementType)=>elementType+"[]";
  public string GetTypeFromDefinition(MetadataReader reader, TypeDefinitionHandle handle, byte rawTypeKind)=>reader.GetString(reader.GetTypeDefinition(handle).Name);
  public string GetTypeFromReference(MetadataReader reader, TypeReferenceHandle handle, byte rawTypeKind)=>reader.GetString(reader.GetTypeReference(handle).Name);
  public string GetTypeFromSpecification(MetadataReader reader, object genericContext, TypeSpecificationHandle handle, byte rawTypeKind)=>"spec";
}
