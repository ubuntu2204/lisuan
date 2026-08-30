using System;
using System.IO;
using System.Text;
using System.Collections.Generic;

// Minimal IL2CPP metadata v27 parser to locate a type and its method's methodIndex + RVA.
// References Il2CppGlobalMetadataHeader (v24+) layout.

class Program
{
    static byte[] data;
    static uint Read32(long o) => BitConverter.ToUInt32(data, (int)o);
    static ushort Read16(long o) => BitConverter.ToUInt16(data, (int)o);

    // Il2CppGlobalMetadataHeader (v21+)
    // offset 0: sanity (4), version (4)
    // then a series of (offset,count) pairs.
    static void Main(string[] args)
    {
        string path = args[0];
        string typeName = args.Length > 1 ? args[1] : "VivoxChatServiceAdapter";
        data = File.ReadAllBytes(path);
        uint sanity = Read32(0);
        uint version = Read32(4);
        Console.WriteLine($"sanity={sanity:x8} version={version}");
        if (sanity != 0xfab11baf) { Console.WriteLine("bad magic"); return; }

        // Il2CppGlobalMetadataHeader v24 fields (each is int32 offset then int32 count), absolute from file start.
        // Layout (v21..v24): 
        // 0 sanity, 4 version
        // 8  stringLiteralOffset, 12 stringLiteralCount
        // 16 stringLiteralDataOffset, 20 stringLiteralDataCount
        // 24 stringOffset, 28 stringCount
        // 32 eventsOffset, 36 eventsCount
        // 40 propertiesOffset, 44 propertiesCount
        // 48 methodsOffset, 52 methodsCount
        // 56 parameterDefaultValuesOffset, 60 ...
        // 64 fieldDefaultValuesOffset
        // 68 fieldAndParameterDefaultValueDataOffset
        // 72 fieldMarshaledSizesOffset
        // 76 delegatesOffset
        // 80 classesOffset, 84 classesCount
        // 84? ...
        // We'll just locate needed tables by known v24 ordering.

        uint stringOffset = Read32(24);
        uint stringCount  = Read32(28);
        uint methodsOffset = Read32(48);
        uint methodsCount  = Read32(52);
        uint classesOffset = Read32(80);
        uint classesCount  = Read32(84);
        uint typeDefinitionsOffset = Read32( (version>=24)? 120u : 96u ); // v24 adds more tables before typeDefinitions
        // Actually for v24+:
        // 88  methodSpecsOffset, 92 methodSpecsCount
        // 96  genericParametersOffset, 100 genericParametersCount
        // 104  genericParameterConstraintsOffset, 108 genericParameterConstraintsCount
        // 112 genericContainersOffset, 116 genericContainersCount
        // 120 nestedTypesOffset, 124 nestedTypesCount
        // 128 interfacesOffset, 132 interfacesCount
        // 136 vtableMethodsOffset, 140 vtableMethodsCount
        // 144 interfaceOffsetsOffset, 148 interfaceOffsetsCount
        // 152 typeDefinitionsOffset, 156 typeDefinitionsCount   <-- for v24
        // For older v21: typeDefinitionsOffset is at 96.

        uint typeDefOffset = (version >= 24) ? Read32(152) : Read32(96);
        uint typeDefCount  = (version >= 24) ? Read32(156) : Read32(100);

        Console.WriteLine($"strings @{stringOffset} count={stringCount}");
        Console.WriteLine($"typeDefs @{typeDefOffset} count={typeDefCount}");
        Console.WriteLine($"methods  @{methodsOffset} count={methodsCount}");
        Console.WriteLine($"classes  @{classesOffset} count={classesCount}");

        string ReadStr(uint off) {
            if (off >= data.Length) return "";
            int i = (int)off; int s = i;
            while (i < data.Length && data[i] != 0) i++;
            return Encoding.UTF8.GetString(data, s, i - s);
        }

        // Il2CppTypeDefinition v27 layout (size 0x70 = 112 bytes in many builds; varies).
        // We need: nameIndex (stringIndex), methodStart (index into methods), method_count, etc.
        // Try size 0x70 first.
        int tdSize = 0x70;
        // fields we need within typeDefinition:
        // 0: nameIndex (uint)
        // 4: namespaceIndex (uint)
        // 8: declaringType (int) , 12: parentType (int)
        // 16: elementType (uint)
        // then flags, etc.
        // methodStart at offset 0x2C? Actually:
        // Common Il2CppTypeDefinition (per Il2CppInspector):
        // off 0x00 nameIndex uint
        // 0x04 namespaceIndex uint
        // 0x08 declaringTypeIndex int32
        // 0x0C parentTypeIndex int32
        // 0x10 elementTypeIndex uint
        // 0x14 genericContainerIndex int32
        // 0x18 flags uint
        // 0x1C fieldStart int32
        // 0x20 field_count uint16 ; 0x22 fieldEnd ? (actually) 
        // 0x20 fieldStart? Let's rely on Il2CppInspector 2020.3 layout:
        // 0x18 flags
        // 0x1C fieldStart (uint)
        // 0x20 fieldCount (uint)
        // 0x24 methodStart (uint)
        // 0x28 methodCount (uint)
        // 0x2C eventStart, 0x30 eventCount
        // 0x34 propertyStart, 0x38 propertyCount
        // 0x3C nestedTypesStart, 0x40 nestedTypesCount
        // 0x44 interfacesStart, 0x48 interfacesCount
        // 0x4C vtableStart, 0x50 vtableCount
        // 0x54 interfaceOffsetsStart, 0x58 interfaceOffsetsCount
        // 0x5C interopDataOffset? 
        // Then more for 2020.3: 0x5C ??? , 0x60 packingSize, 0x62 ... 0x64 ... 0x68 ...
        // The struct is 0x70.

        int nameIdxOff=0, nsIdxOff=4, methodStartOff=0x24, methodCountOff=0x28;

        for (uint t=0; t<typeDefCount; t++) {
            long baseO = typeDefOffset + t*tdSize;
            uint nameIdx = Read32(baseO + nameIdxOff);
            string nm = ReadStr(stringOffset + nameIdx);
            if (nm.Contains(typeName)) {
                uint nsIdx = Read32(baseO + nsIdxOff);
                string ns = ReadStr(stringOffset + nsIdx);
                uint mStart = Read32(baseO + methodStartOff);
                uint mCount = Read32(baseO + methodCountOff);
                Console.WriteLine($"TYPE[{t}] name='{nm}' ns='{ns}' methodStart={mStart} methodCount={mCount}");
                // dump methods
                for (uint m=0; m<mCount; m++) {
                    long mo = methodsOffset + (mStart+m)*10; // Il2CppMethodDefinition size = 10? Actually 0x0A: nameIndex(4)+ declaringType(4)+ returnType(2)? 
                    // Il2CppMethodDefinition: nameIndex(uint32), declaringType(int32), returnType(uint16? actually TypeIndex uint16 in some, but 2020 uses: nameIndex, declaringType, returnType(int16?), parameterStart, parameterCount)
                    // size = 0x0C? Standard 2020.3: 
                    //  0 nameIndex uint
                    //  4 declaringType int32
                    //  8 returnType int16 (or uint16)
                    // 10 parameterStart uint16
                    // 12 parameterCount uint16 (then padding)
                    // size = 0x10
                    uint mname = Read32(mo+0);
                    int decl = (int)Read32(mo+4);
                    ushort ret = Read16(mo+8);
                    uint pStart = Read16(mo+10);
                    ushort pCount = Read16(mo+12);
                    string mn = ReadStr(stringOffset + mname);
                    Console.WriteLine($"   METHOD[{mStart+m}] name='{mn}' ret={ret} params={pCount}");
                    if (mn.Contains("Login") || mn.Contains("Vivox") || mn.Contains("Init") ) {
                        // record methodIndex = mStart+m (this is index used into Il2CppCodeRegistration? not exactly; methodIndex = mStart+m)
                    }
                }
            }
        }
        Console.WriteLine("done");
    }
}
