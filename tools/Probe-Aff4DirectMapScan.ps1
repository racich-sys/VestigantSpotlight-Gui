param(
    [Parameter(Mandatory=$true)][string]$Aff4Input,
    [Parameter(Mandatory=$true)][string]$ThinZip,
    [int]$MaxMapEntries = 50000
)

$ErrorActionPreference = "Stop"

$cs = @'
using System;
using System.IO;
using System.IO.Compression;
using System.Collections.Generic;
using System.Text;

public class VestigantAff4DirectMapScanner {
  struct Row { public string Name; public ulong Local; public ulong Size; }
  static ulong U64(byte[] b,int o){return BitConverter.ToUInt64(b,o);}
  static uint U32(byte[] b,int o){return BitConverter.ToUInt32(b,o);}
  static ushort U16(byte[] b,int o){return BitConverter.ToUInt16(b,o);}
  static ulong Payload(FileStream fs, ulong local){fs.Position=(long)local;byte[] h=new byte[30];fs.Read(h,0,30);if(U32(h,0)!=0x04034b50)throw new Exception("bad local header");return local+30+U16(h,26)+U16(h,28);}
  static byte[] ReadEntry(FileStream fs, Dictionary<string,Row> rows, string name){Row r=rows[name];ulong p=Payload(fs,r.Local);byte[] b=new byte[(int)r.Size];fs.Position=(long)p;int got=0;while(got<b.Length){int n=fs.Read(b,got,b.Length-got);if(n<=0)break;got+=n;}return b;}
  static int RL(byte[] s, ref int p, int b){int l=b;if(b==15){while(true){if(p>=s.Length)throw new Exception("lz4 length overrun");int x=s[p++];l+=x;if(x!=255)break;}}return l;}
  static byte[] Lz4(byte[] s,int exp){byte[] o=new byte[exp];int op=0,p=0;while(p<s.Length){int t=s[p++];int lit=RL(s,ref p,t>>4);if(p+lit>s.Length||op+lit>exp)throw new Exception("lz4 literal overrun");Buffer.BlockCopy(s,p,o,op,lit);p+=lit;op+=lit;if(op==exp||p>=s.Length)break;if(p+2>s.Length)throw new Exception("lz4 offset missing");int mo=s[p]|(s[p+1]<<8);p+=2;if(mo==0||mo>op)throw new Exception("lz4 invalid match");int ml=RL(s,ref p,t&15)+4;if(op+ml>exp)throw new Exception("lz4 output overrun");for(int i=0;i<ml;i++)o[op++]=o[op-mo];if(op==exp)break;}if(op!=exp)throw new Exception("lz4 size mismatch");return o;}
  static void Parse(string text,out Dictionary<string,Row> rows){rows=new Dictionary<string,Row>();string[] lines=text.Split(new string[]{"\r\n","\n"},StringSplitOptions.RemoveEmptyEntries);for(int i=1;i<lines.Length;i++){string[] p=lines[i].Split(',');if(p.Length<9)continue;Row r=new Row();r.Name=p[4].Trim('"');r.Local=UInt64.Parse(p[8].Trim('"'));r.Size=UInt64.Parse(p[7].Trim('"'));rows[r.Name]=r;}}
  public static string Scan(string aff,string zip,int maxEntries){
    string csv="";
    using(ZipArchive z=ZipFile.OpenRead(zip)){foreach(ZipArchiveEntry e in z.Entries){if(e.FullName=="aff4_zip_central_directory.csv"){using(StreamReader sr=new StreamReader(e.Open()))csv=sr.ReadToEnd();break;}}}
    Dictionary<string,Row> rows;Parse(csv,out rows);
    StringBuilder sb=new StringBuilder();
    using(FileStream fs=File.OpenRead(aff)){
      string pre="aff4%3A%2F%2F99930a27-3e61-419e-8b6b-65a3a40bedcb";
      byte[] map=ReadEntry(fs,rows,pre+"/map");
      int limit=Math.Min(map.Length/28,maxEntries);
      Dictionary<string,byte[]> idxs=new Dictionary<string,byte[]>();
      Dictionary<string,ulong> dps=new Dictionary<string,ulong>();
      HashSet<ulong> seen=new HashSet<ulong>();
      int chunks=0,hits=0,err=0;
      for(int i=0;i<limit&&hits<100;i++){
        ulong vo=U64(map,i*28),len=U64(map,i*28+8),so=U64(map,i*28+16);
        uint sid=U32(map,i*28+24);
        if(sid!=0||len==0)continue;
        ulong sc=so/32768UL,ec=(so+len-1)/32768UL;
        for(ulong c=sc;c<=ec&&hits<100;c++){
          if(!seen.Add(c))continue;
          chunks++;
          uint bev=(uint)(c/1024UL);
          int inn=(int)(c%1024UL);
          string seg=bev.ToString("00000000");
          try{
            if(!idxs.ContainsKey(seg))idxs[seg]=ReadEntry(fs,rows,pre+"/data/"+seg+".index");
            byte[] idx=idxs[seg];
            ulong rel=U64(idx,inn*12);
            uint clen=U32(idx,inn*12+8);
            if(!dps.ContainsKey(seg))dps[seg]=Payload(fs,rows[pre+"/data/"+seg].Local);
            byte[] comp=new byte[clen];
            fs.Position=(long)(dps[seg]+rel);
            fs.Read(comp,0,comp.Length);
            byte[] dec=clen==32768U?comp:Lz4(comp,32768);
            for(int pp=32;pp<=32764;pp+=4096){
              bool nx=dec[pp]=='N'&&dec[pp+1]=='X'&&dec[pp+2]=='S'&&dec[pp+3]=='B';
              bool ap=dec[pp]=='A'&&dec[pp+1]=='P'&&dec[pp+2]=='S'&&dec[pp+3]=='B';
              if(nx||ap){
                long delta=(long)(c*32768UL)-(long)so+pp;
                ulong virt=(ulong)((long)vo+delta);
                sb.AppendLine("HIT magic="+(nx?"NXSB":"APSB")+" virtual="+virt+" entry="+i+" chunk="+c+" mapOffset="+vo+" streamOffset="+so+" len="+len);
                hits++;
              }
            }
          }catch(Exception){err++;}
        }
      }
      sb.AppendLine("map_entries="+limit+" chunks="+chunks+" hits="+hits+" errors="+err);
    }
    return sb.ToString();
  }
}
'@

Add-Type -TypeDefinition $cs -ReferencedAssemblies "System.IO.Compression","System.IO.Compression.FileSystem"
[VestigantAff4DirectMapScanner]::Scan($Aff4Input, $ThinZip, $MaxMapEntries)
