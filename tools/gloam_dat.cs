// SPDX-License-Identifier: GPL-3.0-or-later
// Bounded metadata parsing and streaming ROM fingerprints for desktop setup.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Xml;

public sealed class GloamDatEntry
{
    public string Name, RomName, Sha1, Md5, Crc;
    public long Size;
}
public sealed class GloamFingerprint
{
    public string Sha1, Md5, Crc;
    public long Size;
}
public static class GloamDat
{
    static string Hex(byte[] bytes) { return BitConverter.ToString(bytes).Replace("-", "").ToLowerInvariant(); }
    public static GloamFingerprint Fingerprint(string path)
    {
        using (var file = File.OpenRead(path))
        using (var sha = SHA1.Create())
        using (var md5 = MD5.Create()) {
            long size = file.Length;
            if (size > 1024L * 1024 * 1024) throw new InvalidDataException("Identification limit is 1 GiB per file.");
            var table = new uint[256];
            for (uint i = 0; i < 256; i++) {
                uint c = i;
                for (int j = 0; j < 8; j++) c = (c & 1) != 0 ? 0xedb88320U ^ (c >> 1) : c >> 1;
                table[i] = c;
            }
            uint crc = 0xffffffffU;
            var buffer = new byte[65536];
            int read; long total = 0;
            while ((read = file.Read(buffer, 0, buffer.Length)) != 0) {
                total += read;
                if (total > size) throw new IOException("Content changed during identification.");
                sha.TransformBlock(buffer, 0, read, buffer, 0);
                md5.TransformBlock(buffer, 0, read, buffer, 0);
                for (int i = 0; i < read; i++) crc = table[(crc ^ buffer[i]) & 255] ^ (crc >> 8);
            }
            if (total != size) throw new IOException("Content changed during identification.");
            sha.TransformFinalBlock(new byte[0], 0, 0); md5.TransformFinalBlock(new byte[0], 0, 0);
            return new GloamFingerprint { Size=size, Sha1=Hex(sha.Hash), Md5=Hex(md5.Hash), Crc=(crc ^ 0xffffffffU).ToString("x8") };
        }
    }
    static string Value(Dictionary<string,string> fields, string key)
    {
        string result; return fields.TryGetValue(key, out result) ? result : "";
    }
    static void Add(List<GloamDatEntry> output, string title, Dictionary<string,string> rom)
    {
        long size;
        if (String.IsNullOrWhiteSpace(title) || title.Length > 1024 || !Int64.TryParse(Value(rom,"size"), NumberStyles.None, CultureInfo.InvariantCulture, out size) || size < 0) return;
        string sha=Value(rom,"sha1"), md5=Value(rom,"md5"), crc=Value(rom,"crc");
        if (sha.Length > 0 && !Regex.IsMatch(sha,"\\A[0-9a-fA-F]{40}\\z")) return;
        if (md5.Length > 0 && !Regex.IsMatch(md5,"\\A[0-9a-fA-F]{32}\\z")) return;
        if (crc.Length > 0 && !Regex.IsMatch(crc,"\\A[0-9a-fA-F]{8}\\z")) return;
        if (sha.Length == 0 && md5.Length == 0 && crc.Length == 0) return;
        if (output.Count >= 250000) throw new InvalidDataException("DAT exceeds 250,000 entries; select a smaller DAT.");
        output.Add(new GloamDatEntry { Name=title, RomName=Value(rom,"name"), Size=size, Sha1=sha.ToLowerInvariant(), Md5=md5.ToLowerInvariant(), Crc=crc.ToLowerInvariant() });
    }
    public static string Match(GloamDatEntry entry, GloamFingerprint file)
    {
        if (entry.Size != file.Size) return "";
        // A stronger recorded digest must match; never fall back to CRC after
        // a SHA-1/MD5 mismatch in the same record.
        if (entry.Sha1.Length != 0) return entry.Sha1 == file.Sha1 ? "SHA-1 + size" : "";
        if (entry.Md5.Length != 0) return entry.Md5 == file.Md5 ? "MD5 + size" : "";
        return entry.Crc == file.Crc ? "CRC32 + size (weaker)" : "";
    }
    sealed class Node {
        public string Type;
        public Dictionary<string,string> Fields = new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
        public List<Node> Children = new List<Node>();
    }
    sealed class Lexer {
        readonly string text; int offset; int tokens;
        public Lexer(string value) { text=value; }
        public string Next() {
            while (offset < text.Length && Char.IsWhiteSpace(text[offset])) offset++;
            if (offset == text.Length) return null;
            if (++tokens > 4000000) throw new InvalidDataException("DAT token limit exceeded.");
            char c=text[offset++];
            if (c == '(' || c == ')') return c.ToString();
            if (c == '"') {
                var value=new StringBuilder();
                while (offset < text.Length) {
                    c=text[offset++];
                    if (c == '"') return value.ToString();
                    if (c == '\\' && offset < text.Length && (text[offset] == '\\' || text[offset] == '"')) c=text[offset++];
                    value.Append(c);
                }
                throw new InvalidDataException("Unterminated DAT string.");
            }
            int start=offset-1;
            while (offset < text.Length && !Char.IsWhiteSpace(text[offset]) && text[offset] != '(' && text[offset] != ')') offset++;
            return text.Substring(start,offset-start);
        }
    }
    static Node Block(Lexer lexer, string type, int depth) {
        if (depth > 8) throw new InvalidDataException("DAT nesting limit exceeded.");
        var node=new Node {Type=type};
        string key;
        while ((key=lexer.Next()) != ")") {
            if (key == null || key == "(") throw new InvalidDataException("Malformed DAT block.");
            string value=lexer.Next();
            if (value == null || value == ")") throw new InvalidDataException("Missing DAT value.");
            if (value == "(") node.Children.Add(Block(lexer,key,depth+1));
            else node.Fields[key]=value;
        }
        return node;
    }
    public static GloamDatEntry[] Read(string path)
    {
        if (new FileInfo(path).Length > 32 * 1024 * 1024) throw new InvalidDataException("DAT exceeds 32 MiB; select a system-specific file.");
        string text=File.ReadAllText(path).TrimStart('\ufeff',' ','\t','\r','\n');
        var output=new List<GloamDatEntry>();
        if (text.StartsWith("<", StringComparison.Ordinal)) {
            var settings=new XmlReaderSettings { DtdProcessing=DtdProcessing.Ignore, XmlResolver=null, MaxCharactersInDocument=32*1024*1024, MaxCharactersFromEntities=1024 };
            using (var reader=XmlReader.Create(new StringReader(text), settings)) {
                string title="";
                while (reader.Read()) {
                    if (reader.NodeType == XmlNodeType.Element && (reader.Name == "game" || reader.Name == "machine")) title=reader.GetAttribute("name") ?? "";
                    if (reader.NodeType == XmlNodeType.EndElement && (reader.Name == "game" || reader.Name == "machine")) title="";
                    if (reader.NodeType != XmlNodeType.Element || reader.Name != "rom") continue;
                    var fields=new Dictionary<string,string>();
                    foreach (string key in new [] {"name","size","sha1","md5","crc"}) fields[key]=reader.GetAttribute(key) ?? "";
                    Add(output,title,fields);
                }
            }
        } else {
            var lexer=new Lexer(text); string type;
            while ((type=lexer.Next()) != null) {
                if (lexer.Next() != "(") throw new InvalidDataException("Expected a clrmamepro DAT block.");
                var node=Block(lexer,type,0);
                if (type != "game" && type != "machine") continue;
                foreach (var child in node.Children) if (child.Type == "rom") Add(output,Value(node.Fields,"name"),child.Fields);
            }
        }
        if (output.Count == 0) throw new InvalidDataException("No ROM size/hash records found in this DAT.");
        return output.ToArray();
    }
}
