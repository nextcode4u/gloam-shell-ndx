// SPDX-License-Identifier: GPL-3.0-or-later
// Metadata-only search-index writer. Never opens a ROM or follows a linked entry.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

public static class GloamLibraryIndex
{
    const long Limit = 64L * 1024 * 1024;
    static readonly UTF8Encoding Utf8 = new UTF8Encoding(false, true);
    static readonly HashSet<string> Support = new HashSet<string>(StringComparer.OrdinalIgnoreCase) {
        "save", "saves", "savestates", "bios", "__pycache__", ".git", "$RECYCLE.BIN", "System Volume Information"
    };
    static readonly HashSet<string> SaveTypes = new HashSet<string>(StringComparer.OrdinalIgnoreCase) {
        ".sav", ".srm", ".dsv", ".pub", ".prv", ".bak", ".tmp", ".state"
    };
    sealed class Block {
        public uint Offset, First, Count, Bytes;
        public byte[] Filter = new byte[256];
    }
    static uint Fold(uint c) {
        if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7) ||
            (c >= 0x391 && c <= 0x3AB && c != 0x3A2) || (c >= 0x410 && c <= 0x42F)) return c + 32;
        return c == 0x401 ? 0x451U : c;
    }
    static void AddFilter(byte[] filter, string path) {
        uint a = 0, b = 0;
        int seen = 0;
        for (int i = 0; i < path.Length; i++) {
            uint c = Fold((uint)Char.ConvertToUtf32(path, i));
            if (Char.IsHighSurrogate(path[i])) i++;
            if (seen >= 2) {
                uint hash = unchecked(((2166136261U ^ a) * 16777619U ^ b) * 16777619U);
                hash = unchecked((hash ^ c) * 16777619U);
                uint first = hash & 2047, second = (hash >> 11) & 2047;
                filter[first / 8] |= (byte)(1 << (int)(first % 8));
                filter[second / 8] |= (byte)(1 << (int)(second % 8));
            }
            a = b; b = c; seen++;
        }
    }
    static void Cancel(string file)
    {
        if (!String.IsNullOrEmpty(file) && File.Exists(file))
            throw new OperationCanceledException("Search index cancelled; the installed index was kept.");
    }
    public static uint Hash(byte[] bytes, int count, uint hash)
    {
        for (int i = 0; i < count; i++) hash = unchecked((hash ^ bytes[i]) * 16777619U);
        return hash;
    }
    static byte Flags(string relative, FileAttributes attributes, byte inherited)
    {
        byte flags = inherited;
        if ((attributes & FileAttributes.Hidden) != 0 || relative.StartsWith(".") || relative.Contains("/.")) flags |= 1;
        if ((attributes & FileAttributes.System) != 0) flags |= 2;
        return flags;
    }
    static string[] Entries(string directory, bool folders, string cancel)
    {
        var entries = new List<string>();
        var source = folders ? Directory.EnumerateDirectories(directory) : Directory.EnumerateFiles(directory);
        foreach (string entry in source) {
            Cancel(cancel);
            if (entries.Count == 1000000) throw new IOException("One directory exceeds the index entry limit.");
            entries.Add(entry);
        }
        entries.Sort(StringComparer.OrdinalIgnoreCase);
        return entries.ToArray();
    }
    static void ReserveBlockDirectory(FileStream stream, uint payload, int tableBytes, string cancel)
    {
        // Make room before the records. Copy backwards so source/destination may overlap.
        stream.Flush();
        stream.SetLength(32L + tableBytes + payload);
        byte[] buffer = new byte[65536];
        long remaining = payload;
        while (remaining > 0) {
            Cancel(cancel);
            int size = (int)Math.Min(remaining, buffer.Length), read = 0;
            long source = 32 + remaining - size;
            stream.Position = source;
            while (read < size) {
                int got = stream.Read(buffer, read, size-read);
                if (got == 0) throw new IOException("Prepared index was truncated.");
                read += got;
            }
            stream.Position = source + tableBytes;
            stream.Write(buffer, 0, size);
            remaining -= size;
        }
        stream.Position = 32;
    }
    public static uint Build(string root, string[] libraries, string output, string cancel)
    {
        root = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        uint count = 0;
        var blocks = new List<Block>();
        long visited = 0, skipped = 0;
        var stack = new Stack<KeyValuePair<string, byte>>();
        for (int i = libraries.Length - 1; i >= 0; i--) {
            string folder = Path.GetFullPath(Path.Combine(root, libraries[i]));
            if (!folder.StartsWith(root, StringComparison.OrdinalIgnoreCase)) throw new IOException("Library leaves the SD root.");
            if (Directory.Exists(folder)) stack.Push(new KeyValuePair<string, byte>(folder, 0));
        }
        using (var stream = new FileStream(output, FileMode.CreateNew, FileAccess.ReadWrite, FileShare.None))
        using (var writer = new BinaryWriter(stream, Utf8)) {
            writer.Write(new byte[32]);
            while (stack.Count > 0) {
                Cancel(cancel);
                var item = stack.Pop();
                FileAttributes directoryAttributes = File.GetAttributes(item.Key);
                if ((directoryAttributes & FileAttributes.ReparsePoint) != 0) { skipped++; continue; }
                string directoryRelative = item.Key.Substring(root.Length).Replace('\\', '/');
                byte parentFlags = Flags(directoryRelative, directoryAttributes, item.Value);
                string[] folders = Entries(item.Key, true, cancel);
                for (int i = folders.Length - 1; i >= 0; i--) {
                    if (Support.Contains(Path.GetFileName(folders[i]))) { skipped++; continue; }
                    stack.Push(new KeyValuePair<string, byte>(folders[i], parentFlags));
                    if (stack.Count > 1000000) throw new IOException("Too many pending library folders.");
                }
                foreach (string file in Entries(item.Key, false, cancel)) {
                    Cancel(cancel);
                    visited++;
                    FileAttributes attr = File.GetAttributes(file);
                    string name = Path.GetFileName(file), extension = Path.GetExtension(name);
                    if ((attr & FileAttributes.ReparsePoint) != 0 || SaveTypes.Contains(extension) ||
                        ((name.Equals("README.txt", StringComparison.OrdinalIgnoreCase) || name.Equals("README.md", StringComparison.OrdinalIgnoreCase) || name.Equals("README", StringComparison.OrdinalIgnoreCase))) ||
                        name.Equals("cover.png", StringComparison.OrdinalIgnoreCase) ||
                        name.Equals("cover.jpg", StringComparison.OrdinalIgnoreCase) ||
                        name.Equals("cover.jpeg", StringComparison.OrdinalIgnoreCase) ||
                        ((extension.Equals(".png", StringComparison.OrdinalIgnoreCase) ||
                          extension.Equals(".jpg", StringComparison.OrdinalIgnoreCase) ||
                          extension.Equals(".jpeg", StringComparison.OrdinalIgnoreCase)) &&
                         File.Exists(Path.Combine(Path.GetDirectoryName(file), Path.GetFileNameWithoutExtension(file))))) {
                        skipped++; continue;
                    }
                    string relative = file.Substring(root.Length).Replace('\\', '/');
                    byte[] path = Utf8.GetBytes(relative);
                    if (path.Length > 759 || Utf8.GetByteCount(name) > 255 || relative.IndexOf(':') >= 0) { skipped++; continue; }
                    foreach (char c in relative) if (c < 32) throw new IOException("Control character in a library path.");
                    if (count == 1000000 || stream.Position + 8 + path.Length + ((count + 1 + 63) / 64) * 276L > Limit)
                        throw new IOException("Search index exceeds its 64 MiB / one-million-file limit.");
                    if (count % 64 == 0) blocks.Add(new Block { Offset=(uint)stream.Position, First=count });
                    Block block = blocks[blocks.Count - 1];
                    block.Count++;
                    block.Bytes += (uint)(8 + path.Length);
                    AddFilter(block.Filter, relative);
                    byte flags = Flags(relative, attr, parentFlags);
                    uint hash = Hash(new byte[] { flags, 0 }, 2, 2166136261U);
                    hash = Hash(path, path.Length, hash);
                    writer.Write((ushort)path.Length);
                    writer.Write((ushort)flags);
                    writer.Write(hash);
                    writer.Write(path);
                    count++;
                    if (visited % 1000 == 0) Console.WriteLine("Search index: " + visited + " files visited, " + count + " indexed.");
                }
            }
            Cancel(cancel);
            uint payload = checked((uint)(stream.Position - 32));
            int tableBytes = checked(blocks.Count * 276);
            ReserveBlockDirectory(stream, payload, tableBytes, cancel);
            foreach (Block block in blocks) {
                Cancel(cancel);
                byte[] entry;
                using (var memory = new MemoryStream())
                using (var fields = new BinaryWriter(memory)) {
                    fields.Write(block.Offset + (uint)tableBytes); fields.Write(block.First);
                    fields.Write(block.Count); fields.Write(block.Bytes); fields.Write(block.Filter);
                    fields.Flush(); entry = memory.ToArray();
                }
                writer.Write(entry); writer.Write(Hash(entry, entry.Length, 2166136261U));
            }
            byte[] header;
            using (var memory = new MemoryStream())
            using (var meta = new BinaryWriter(memory)) {
                meta.Write(Encoding.ASCII.GetBytes("GLIDX002"));
                meta.Write(count);
                meta.Write(payload);
                meta.Write((ulong)(DateTime.UtcNow - new DateTime(1970, 1, 1)).TotalSeconds);
                meta.Write((uint)blocks.Count);
                meta.Flush();
                header = memory.ToArray();
            }
            stream.Position = 0;
            writer.Write(header);
            writer.Write(Hash(header, header.Length, 2166136261U));
            writer.Flush();
            stream.Flush(true);
        }
        Console.WriteLine("Search index prepared: " + count + " files; " + skipped + " support/linked/oversized entries skipped.");
        return count;
    }
}
