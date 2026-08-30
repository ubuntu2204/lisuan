import struct, sys

def main():
    path=sys.argv[1]
    data=open(path,'rb').read()
    # find COR20 header via CLI header
    # PE: e_lfanew at 0x3c
    e_lfanew=struct.unpack_from("<I",data,0x3c)[0]
    # Optional header at e_lfanew+24; COR header RVA at optional header + 0xE0 (224) for PE32
    oh=e_lfanew+24
    # data directories start at oh + (image optional header size). For PE32, NumberOfRvaAndSizes at oh+92
    numdir=struct.unpack_from("<I",data,oh+92)[0]
    # first data dir is Export, 15th (index 14) is COM Descriptor (CLI)
    # Actually COM descriptor is data directory index 14 (0xE) for 32-bit
    # offset = oh + 96 + 14*8
    cli_rva=struct.unpack_from("<I",data,oh+96+14*8)[0]
    cli_size=struct.unpack_from("<I",data,oh+96+14*8+4)[0]
    # convert RVA to file offset: need section. Use simple: find section containing cli_rva
    # section headers at oh+optsize; optsize = oh? compute from PE: oh+0x10 has SizeOfOptionalHeader
    optsize=struct.unpack_from("<H",data,oh+0x10)[0]
    sh=oh+optsize
    numsec=struct.unpack_from("<H",data,e_lfanew+4+2)[0]
    def rva2off(rva):
        for i in range(numsec):
            s=sh+i*40
            vaddr=struct.unpack_from("<I",data,s+12)[0]
            vsize=struct.unpack_from("<I",data,s+8)[0]
            rawptr=struct.unpack_from("<I",data,s+20)[0]
            if vaddr<=rva<vaddr+vsize:
                return rawptr+(rva-vaddr)
        return None
    cf=rva2off(cli_rva)
    # CLI header: cb(4), majmin(2), flags(2), Streams(2) ... then metadata RVA/size at +8
    md_rva=struct.unpack_from("<I",data,cf+8)[0]
    md_size=struct.unpack_from("<I",data,cf+12)[0]
    mdf=rva2off(md_rva)
    # metadata root: sig(4), flags(2), version(2), version string..., then stream headers
    # streams start after: 4+2+2 + version length (padded to 4)
    verlen=struct.unpack_from("<I",data,mdf+4)[0]  # actually version string length is at +12? 
    # Layout: 0 sig, 4 flags, 6 major, 8 minor, 10 reserved, 12 versionLength, 14 versionString...
    vlen=struct.unpack_from("<I",data,mdf+12)[0]
    pos=mdf+16
    if vlen%4!=0: vlen+=4-vlen%4
    pos+=vlen
    # stream headers: each: offset(4) size(4) name(nul-terminated, padded to 4)
    streams={}
    while pos < mdf+md_size:
        off=struct.unpack_from("<I",data,pos)[0]
        size=struct.unpack_from("<I",data,pos+4)[0]
        nameoff=pos+8
        end=data.index(b'\x00',nameoff)
        name=data[nameoff:end].decode('utf-8','replace')
        streams[name]=(mdf+off,size)
        pos+=8+(len(name)+1)
        if (len(name)+1)%4!=0: pos+=4-((len(name)+1)%4)
        if len(streams)>=6: break
    # strings and blob and tables
    strings_off,strings_size=streams.get('#Strings',(0,0))
    tabledef_off=streams.get('#~',(0,0))[0]
    # parse #~ tables: after header (off,size,valid, sorted), then table rows
    p=tabled_off
    reserved=struct.unpack_from("<I",data,p)[0]
    major=struct.unpack_from("<H",data,p+4)[0]
    minor=struct.unpack_from("<H",data,p+6)[0]
    valid=struct.unpack_from("<Q",data,p+8)[0]
    sorted_=struct.unpack_from("<Q",data,p+16)[0]
    rows={}
    bit=0
    pp=p+24
    for t in range(64):
        if valid&(1<<t):
            rows[t]=struct.unpack_from("<I",data,pp)[0]
            pp+=4
    # TypeDef table index = 2
    if 2 not in rows: 
        print("no TypeDef"); return
    td_rows=rows[2]
    # TypeDef row: Flags(2) TypeName(2->#Strings) TypeNamespace(2->#Strings) Extends(4 coded) FieldList(2) MethodList(2)
    td_off=tabled_off + (pp-p)  # tables start after row counts
    # Actually tables blob starts at pp (after the row-count array)
    tables_start=pp
    row_size=2+2+2+4+2+2
    for i in range(td_rows):
        ro=tables_start+i*row_size
        flags=struct.unpack_from("<H",data,ro)[0]
        name_idx=struct.unpack_from("<H",data,ro+2)[0]
        ns_idx=struct.unpack_from("<H",data,ro+4)[0]
        # get strings
        def getstr(base,idx):
            po=base+idx
            e=data.index(b'\x00',po)
            return data[po:e].decode('utf-8','replace')
        nm=getstr(strings_off,name_idx)
        ns=getstr(strings_off,ns_idx)
        if 'BasePlugin' in nm or 'Plugin' in nm and 'BepIn' in ns:
            print("TYPE:",ns,".",nm)
main()
