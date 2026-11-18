// filesystem.cpp - in-memory file system where every write keeps a new version, plus a trash bin.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#include "filesystem.h"
#include "utils.h"
#include "kernel_log.h"
#include "types.h"
#include "globals.h"

// What it is: a name finder for the current folder.
// What it does: returns the index of the live file or folder called `name` in
// the current folder, or -1 if there is none. Trashed files are ignored.
int find_in_cwd(string name) {
    for (int i = 1; i < MAX_NODES; i++) {
        if (nodes[i].used && !nodes[i].deleted && nodes[i].parent == cwd && nodes[i].name == name) {
            return i;
        }
    }
    return -1;
}

// What it is: the version rebuilder.
// What it does: returns the full text of version `v` (0-based) of file `n`.
// It starts at the nearest full copy and applies the deltas one by one.
string version_text(int n, int v) {
    vector<Version> &vs = nodes[n].versions;
    int k = v;
    while (k > 0 && !vs[k].full) {
        k--;
    }
    string text = vs[k].data;
    for (int j = k + 1; j <= v; j++) {
        text = text.substr(0, vs[j].prefix) + vs[j].data + text.substr(text.size() - vs[j].suffix);
    }
    return text;
}

// Latest text of a file ("" if it has no version).
string latest_text(int n) {
    if (nodes[n].versions.empty()) {
        return "";
    }
    return version_text(n, nodes[n].versions.size() - 1);
}

// What it is: the version deleter.
// What it does: removes version `v` (0-based). If the next version was a delta
// that depended on it, that one is first turned into a full copy so nothing breaks.
void delete_version(int n, int v) {
    vector<Version> &vs = nodes[n].versions;
    if (v + 1 < (int) vs.size() && !vs[v + 1].full) {
        string text = version_text(n, v + 1);
        vs[v + 1].full = true;
        vs[v + 1].prefix = 0;
        vs[v + 1].suffix = 0;
        vs[v + 1].data = text;
    }
    vs.erase(vs.begin() + v);
}

// What it is: the version saver (this is what keeps memory use low).
// What it does: saves `text` as a new version of file `n`.
//  - if the text equals the latest version nothing is stored (returns false)
//  - otherwise only the changed middle part is stored (a delta), with a full
//    copy every KEYFRAME_EVERY versions
//  - if the file now has more than MAX_VERSIONS versions the oldest is dropped
bool add_version(int n, const string &text) {
    vector<Version> &vs = nodes[n].versions;
    Version nv;
    nv.tick = current_tick;
    nv.full = true;
    nv.prefix = 0;
    nv.suffix = 0;
    nv.data = text;
    if (!vs.empty()) {
        string prev = latest_text(n);
        if (prev == text) {
            return false;
        }
        size_t limit = prev.size() < text.size() ? prev.size() : text.size();
        size_t pre = 0;
        while (pre < limit && prev[pre] == text[pre]) {
            pre++;
        }
        size_t suf = 0;
        while (suf < limit - pre && prev[prev.size() - 1 - suf] == text[text.size() - 1 - suf]) {
            suf++;
        }
        int since_full = 0;
        for (int k = (int) vs.size() - 1; k >= 0 && !vs[k].full; k--) {
            since_full++;
        }
        string middle = text.substr(pre, text.size() - pre - suf);
        if (since_full + 1 < KEYFRAME_EVERY && middle.size() < text.size()) {
            nv.full = false;
            nv.prefix = pre;
            nv.suffix = suf;
            nv.data = middle;
        }
    }
    vs.push_back(nv);
    while ((int) vs.size() > MAX_VERSIONS) {
        delete_version(n, 0);
    }
    return true;
}

// Bytes really stored for a file (what the versions cost in memory).
int stored_bytes(int n) {
    int total = 0;
    for (size_t v = 0; v < nodes[n].versions.size(); v++) {
        total += nodes[n].versions[v].data.size();
    }
    return total;
}

// Bytes the versions would cost if every one was kept as a full copy.
int full_bytes(int n) {
    int total = 0;
    for (size_t v = 0; v < nodes[n].versions.size(); v++) {
        total += version_text(n, v).size();
    }
    return total;
}

// What it is: a path builder.
// What it does: returns the full path of the current folder, like "/docs/notes".
string get_cwd_path() {
    string path = "";
    int n = cwd;
    while (n != 0) {
        path = "/" + nodes[n].name + path;
        n = nodes[n].parent;
    }
    if (path == "") {
        path = "/";
    }
    return path;
}

// What it is: a file/folder creator.
// What it does: takes a free slot in the node table and fills it in as a new
// file (starting with one empty version) or folder inside the current folder.
// Returns its index, or -1 if the table is full.
int create_node(string name, bool is_dir) {
    for (int i = 1; i < MAX_NODES; i++) {
        if (!nodes[i].used) {
            nodes[i].used = true;
            nodes[i].is_dir = is_dir;
            nodes[i].deleted = false;
            nodes[i].deleted_seq = 0;
            nodes[i].deleted_tick = 0;
            nodes[i].name = name;
            nodes[i].parent = cwd;
            nodes[i].versions.clear();
            if (!is_dir) {
                add_version(i, "");
            }
            return i;
        }
    }
    return -1;
}

// Command "pwd": prints the current folder.
void cmd_pwd() {
    printf("%s\n", get_cwd_path().c_str());
}

// Command "ls": lists the files and folders in the current folder, with the
// number of versions, size and time of the last change.
void cmd_ls() {
    int count = 0;
    for (int i = 1; i < MAX_NODES; i++) {
        if (nodes[i].used && !nodes[i].deleted && nodes[i].parent == cwd) {
            count++;
            if (nodes[i].is_dir) {
                printf("  [dir]  %s\n", nodes[i].name.c_str());
            } else {
                int v = nodes[i].versions.size();
                printf("  %-16s v%-3d %5d bytes   last change at t=%d\n", nodes[i].name.c_str(), v,
                       (int) latest_text(i).size(), nodes[i].versions[v - 1].tick);
            }
        }
    }
    if (count == 0) {
        printf("  (empty)\n");
    }
}

// Command "mkdir <name>": creates a new folder here.
void cmd_mkdir(vector<string> &args) {
    if (args.size() < 2) {
        printf("usage: mkdir <name>\n");
        return;
    }
    if (args[1].find('/') != string::npos || args[1] == ".." || args[1].size() > 30) {
        printf("mkdir: invalid name (no '/', maximum 30 characters)\n");
        return;
    }
    if (find_in_cwd(args[1]) != -1) {
        printf("mkdir: '%s' already exists\n", args[1].c_str());
        return;
    }
    if (create_node(args[1], true) == -1) {
        printf("mkdir: file system is full\n");
        return;
    }
    log_event("folder created: " + args[1] + " in " + get_cwd_path());
}

// Command "cd <folder>": moves into a folder. "cd .." goes up, "cd /" or just
// "cd" goes to the root.
void cmd_cd(vector<string> &args) {
    if (args.size() < 2 || args[1] == "/") {
        cwd = 0;
        return;
    }
    if (args[1] == "..") {
        cwd = nodes[cwd].parent;
        return;
    }
    int i = find_in_cwd(args[1]);
    if (i == -1 || !nodes[i].is_dir) {
        printf("cd: no such folder '%s'\n", args[1].c_str());
        return;
    }
    cwd = i;
}

// Removes folder `n` and everything below it for good (live or trashed).
static int remove_tree(int n) {
    int count = 0;
    for (int k = 1; k < MAX_NODES; k++) {
        if (nodes[k].used && nodes[k].parent == n && k != n) {
            if (nodes[k].is_dir) {
                count += remove_tree(k);
            } else {
                nodes[k].used = false;
                nodes[k].versions.clear();
                count++;
            }
        }
    }
    nodes[n].used = false;
    return count;
}

// Command "rmdir <folder>": removes an EMPTY folder for good (and clears any
// trashed files that came from it). "rmdir -r <folder>" removes it together with
// everything inside it, permanently.
void cmd_rmdir(vector<string> &args) {
    bool recursive = args.size() >= 3 && args[1] == "-r";
    if (args.size() < 2 || (args[1] == "-r" && args.size() < 3)) {
        printf("usage: rmdir [-r] <folder>\n");
        return;
    }
    string name = recursive ? args[2] : args[1];
    int i = find_in_cwd(name);
    if (i == -1 || !nodes[i].is_dir) {
        printf("rmdir: no such folder '%s'\n", name.c_str());
        return;
    }
    if (recursive) {
        bool inside = false;
        for (int n = cwd; n != 0; n = nodes[n].parent) {
            if (n == i) {
                inside = true;
            }
        }
        if (inside) {
            printf("rmdir: cannot remove the folder you are in\n");
            return;
        }
        int files = remove_tree(i);
        printf("folder '%s' removed with %d item(s) inside\n", name.c_str(), files);
        log_event("folder removed recursively: " + name);
        return;
    }
    for (int k = 1; k < MAX_NODES; k++) {
        if (nodes[k].used && !nodes[k].deleted && nodes[k].parent == i) {
            printf("rmdir: folder '%s' is not empty (use rmdir -r to remove it with its content)\n", name.c_str());
            return;
        }
    }
    for (int k = 1; k < MAX_NODES; k++) {
        if (nodes[k].used && nodes[k].deleted && nodes[k].parent == i) {
            nodes[k].used = false;
            nodes[k].versions.clear();
        }
    }
    nodes[i].used = false;
    log_event("folder removed: " + name);
}

// Command "touch <file>": creates an empty file (version 1) if it is not there.
void cmd_touch(vector<string> &args) {
    if (args.size() < 2) {
        printf("usage: touch <file>\n");
        return;
    }
    if (args[1].find('/') != string::npos || args[1] == ".." || args[1].size() > 30) {
        printf("touch: invalid name (no '/', maximum 30 characters)\n");
        return;
    }
    if (find_in_cwd(args[1]) != -1) {
        printf("touch: '%s' already exists\n", args[1].c_str());
        return;
    }
    if (create_node(args[1], false) == -1) {
        printf("touch: file system is full\n");
        return;
    }
    log_event("file created: " + args[1]);
}

// Commands "write <file> <text>" and "append <file> <text>": save text into a
// file. NOTHING IS EVER OVERWRITTEN: each call stores a NEW version, so older
// versions stay available. "write" replaces the content, "append" adds to the
// end of the latest version. The file is created if it does not exist. Use \n
// inside the text for a new line.
void cmd_write(vector<string> &args, string line, bool append) {
    const char *cmdname = append ? "append" : "write";
    if (args.size() < 3) {
        printf("usage: %s <file> <text>\n", cmdname);
        return;
    }
    if (args[1].find('/') != string::npos || args[1] == ".." || args[1].size() > 30) {
        printf("%s: invalid file name (no '/', maximum 30 characters)\n", cmdname);
        return;
    }
    int i = find_in_cwd(args[1]);
    if (i != -1 && nodes[i].is_dir) {
        printf("%s: '%s' is a folder\n", cmdname, args[1].c_str());
        return;
    }
    bool is_new = false;
    if (i == -1) {
        i = create_node(args[1], false);
        if (i == -1) {
            printf("%s: file system is full\n", cmdname);
            return;
        }
        // a brand new file starts directly with the text we write (no empty version 1)
        nodes[i].versions.clear();
        is_new = true;
    }
    string text = get_text_after(line, 2);
    if (append && !is_new) {
        text = latest_text(i) + text;
    }
    if (!add_version(i, text)) {
        printf("'%s' unchanged, no new version saved\n", args[1].c_str());
        return;
    }
    printf("saved '%s' as version %d (%d bytes)\n", args[1].c_str(), (int) nodes[i].versions.size(), (int) text.size());
    log_event("file " + args[1] + " saved as version " + to_string(nodes[i].versions.size()));
}

// Command "cat <file>": prints the latest version of a file.
void cmd_cat(vector<string> &args) {
    if (args.size() < 2) {
        printf("usage: cat <file>\n");
        return;
    }
    int i = find_in_cwd(args[1]);
    if (i == -1 || nodes[i].is_dir) {
        printf("cat: no such file '%s'\n", args[1].c_str());
        return;
    }
    string text = latest_text(i);
    if (text == "") {
        printf("(empty file)\n");
    } else {
        printf("%s\n", text.c_str());
    }
}

// Command "versions <file>": TIME TRAVEL - lists every saved version of a file
// with the tick it was saved at, its size, and a short preview.
void cmd_versions(vector<string> &args) {
    if (args.size() < 2) {
        printf("usage: versions <file>\n");
        return;
    }
    int i = find_in_cwd(args[1]);
    if (i == -1 || nodes[i].is_dir) {
        printf("versions: no such file '%s'\n", args[1].c_str());
        return;
    }
    for (size_t v = 0; v < nodes[i].versions.size(); v++) {
        string text = version_text(i, v);
        string preview = "";
        for (size_t k = 0; k < text.size() && k < 30; k++) {
            if (text[k] == '\n') {
                preview += "\\n";
            } else {
                preview += text[k];
            }
        }
        if (text.size() > 30) {
            preview += "...";
        }
        printf("  v%-3d t=%-5d %4d bytes   \"%s\"\n", (int) v + 1, nodes[i].versions[v].tick, (int) text.size(), preview.c_str());
    }
}

// Command "restore <file> <n>": brings back the content of version n. The old
// version is not touched - the restored content is saved as a brand new
// latest version, so history is never lost.
void cmd_restore(vector<string> &args) {
    if (args.size() < 3) {
        printf("usage: restore <file> <version number>\n");
        return;
    }
    int i = find_in_cwd(args[1]);
    if (i == -1 || nodes[i].is_dir) {
        printf("restore: no such file '%s'\n", args[1].c_str());
        return;
    }
    int v = atoi(args[2].c_str());
    if (v < 1 || v > (int) nodes[i].versions.size()) {
        printf("restore: '%s' has versions 1 to %d\n", args[1].c_str(), (int) nodes[i].versions.size());
        return;
    }
    string text = version_text(i, v - 1);
    if (!add_version(i, text)) {
        printf("restore: version %d is already the current content, nothing to do\n", v);
        return;
    }
    printf("restored version %d of '%s' as the new version %d\n", v, args[1].c_str(), (int) nodes[i].versions.size());
    log_event("file " + args[1] + " restored from version " + to_string(v));
}

// Command "diff <file> <a> <b>": compares two versions line by line (same line
// number against same line number) and prints '-' for the old line and '+'
// for the new one.
void cmd_diff(vector<string> &args) {
    if (args.size() < 4) {
        printf("usage: diff <file> <version a> <version b>\n");
        return;
    }
    int i = find_in_cwd(args[1]);
    if (i == -1 || nodes[i].is_dir) {
        printf("diff: no such file '%s'\n", args[1].c_str());
        return;
    }
    int a = atoi(args[2].c_str());
    int b = atoi(args[3].c_str());
    int total = nodes[i].versions.size();
    if (a < 1 || a > total || b < 1 || b > total) {
        printf("diff: '%s' has versions 1 to %d\n", args[1].c_str(), total);
        return;
    }
    vector<string> lines_a;
    vector<string> lines_b;
    string one_line;
    istringstream sa(version_text(i, a - 1));
    while (getline(sa, one_line, '\n')) {
        lines_a.push_back(one_line);
    }
    istringstream sb(version_text(i, b - 1));
    while (getline(sb, one_line, '\n')) {
        lines_b.push_back(one_line);
    }
    size_t most = lines_a.size();
    if (lines_b.size() > most) {
        most = lines_b.size();
    }
    int differences = 0;
    printf("diff %s: v%d -> v%d\n", args[1].c_str(), a, b);
    for (size_t k = 0; k < most; k++) {
        bool has_a = k < lines_a.size();
        bool has_b = k < lines_b.size();
        if (has_a && has_b && lines_a[k] == lines_b[k]) {
            printf("    %s\n", lines_a[k].c_str());
        } else {
            differences++;
            if (has_a) {
                printf("  - %s\n", lines_a[k].c_str());
            }
            if (has_b) {
                printf("  + %s\n", lines_b[k].c_str());
            }
        }
    }
    if (differences == 0) {
        printf("  (no differences)\n");
    }
}

// Command "rm <file>": does NOT really delete. The file goes to the trash
// (recycle bin) with all its versions, and can be brought back with 'recover'.
void cmd_rm(vector<string> &args) {
    if (args.size() < 2) {
        printf("usage: rm <file>\n");
        return;
    }
    int i = find_in_cwd(args[1]);
    if (i == -1) {
        printf("rm: no such file '%s'\n", args[1].c_str());
        return;
    }
    if (nodes[i].is_dir) {
        printf("rm: '%s' is a folder, use rmdir\n", args[1].c_str());
        return;
    }
    nodes[i].deleted = true;
    delete_counter++;
    nodes[i].deleted_seq = delete_counter;
    nodes[i].deleted_tick = current_tick;
    printf("'%s' moved to trash (bring it back with: recover %s)\n", args[1].c_str(), args[1].c_str());
    log_event("file moved to trash: " + args[1]);
}

// Command "trash": lists files in the trash and the folder they came from.
void cmd_trash() {
    int count = 0;
    for (int i = 1; i < MAX_NODES; i++) {
        if (nodes[i].used && nodes[i].deleted) {
            count++;
            string from = "/";
            if (nodes[i].parent != 0) {
                from = nodes[nodes[i].parent].name;
            }
            printf("  %-16s from %-12s %d version(s), deleted at t=%d\n", nodes[i].name.c_str(), from.c_str(),
                   (int) nodes[i].versions.size(), nodes[i].deleted_tick);
        }
    }
    if (count == 0) {
        printf("  (trash is empty)\n");
    }
}

// Command "recover <file>": takes the most recently trashed file with that
// name out of the trash and puts it back in its original folder.
void cmd_recover(vector<string> &args) {
    if (args.size() < 2) {
        printf("usage: recover <file>\n");
        return;
    }
    int best = -1;
    for (int i = 1; i < MAX_NODES; i++) {
        if (nodes[i].used && nodes[i].deleted && nodes[i].name == args[1]) {
            if (best == -1 || nodes[i].deleted_seq > nodes[best].deleted_seq) {
                best = i;
            }
        }
    }
    if (best == -1) {
        printf("recover: '%s' is not in the trash\n", args[1].c_str());
        return;
    }
    for (int i = 1; i < MAX_NODES; i++) {
        if (nodes[i].used && !nodes[i].deleted && nodes[i].parent == nodes[best].parent && nodes[i].name == args[1]) {
            printf("recover: a file named '%s' already exists in that folder\n", args[1].c_str());
            return;
        }
    }
    nodes[best].deleted = false;
    printf("'%s' recovered with all %d version(s)\n", args[1].c_str(), (int) nodes[best].versions.size());
    log_event("file recovered from trash: " + args[1]);
}

// Command "emptytrash": permanently deletes everything in the trash.
void cmd_emptytrash() {
    int count = 0;
    for (int i = 1; i < MAX_NODES; i++) {
        if (nodes[i].used && nodes[i].deleted) {
            nodes[i].used = false;
            nodes[i].versions.clear();
            count++;
        }
    }
    printf("Trash emptied (%d file(s) permanently deleted).\n", count);
    log_event("trash emptied");
}

// Command "rmver <file> <n>": permanently deletes ONE saved version of a file.
// The other versions keep their numbers shifted down (v3 becomes v2 if v2 went).
// The last remaining version cannot be deleted (use rm / purge for the file).
void cmd_rmver(vector<string> &args) {
    if (args.size() < 3) {
        printf("usage: rmver <file> <version number>\n");
        return;
    }
    int i = find_in_cwd(args[1]);
    if (i == -1 || nodes[i].is_dir) {
        printf("rmver: no such file '%s'\n", args[1].c_str());
        return;
    }
    int v = atoi(args[2].c_str());
    int total = nodes[i].versions.size();
    if (v < 1 || v > total) {
        printf("rmver: '%s' has versions 1 to %d\n", args[1].c_str(), total);
        return;
    }
    if (total == 1) {
        printf("rmver: that is the only version, use rm or purge to delete the file\n");
        return;
    }
    delete_version(i, v - 1);
    printf("version %d of '%s' deleted (%d version(s) left)\n", v, args[1].c_str(), total - 1);
    log_event("file " + args[1] + " version " + to_string(v) + " deleted");
}

// Command "prune <file> <keep>": deletes the oldest versions so only the newest
// `keep` remain (frees memory).
void cmd_prune(vector<string> &args) {
    if (args.size() < 3) {
        printf("usage: prune <file> <how many newest versions to keep>\n");
        return;
    }
    int i = find_in_cwd(args[1]);
    if (i == -1 || nodes[i].is_dir) {
        printf("prune: no such file '%s'\n", args[1].c_str());
        return;
    }
    int keep = atoi(args[2].c_str());
    if (keep < 1) {
        printf("prune: keep must be at least 1\n");
        return;
    }
    int before = stored_bytes(i);
    int removed = 0;
    while ((int) nodes[i].versions.size() > keep) {
        delete_version(i, 0);
        removed++;
    }
    printf("pruned '%s': %d old version(s) deleted, %d kept, %d bytes freed\n", args[1].c_str(), removed,
           (int) nodes[i].versions.size(), before - stored_bytes(i));
    if (removed > 0) {
        log_event("file " + args[1] + " pruned to " + to_string(keep) + " version(s)");
    }
}

// Command "purge <file>": deletes a file and all its versions for good,
// skipping the trash bin.
void cmd_purge(vector<string> &args) {
    if (args.size() < 2) {
        printf("usage: purge <file>\n");
        return;
    }
    int i = find_in_cwd(args[1]);
    if (i == -1 || nodes[i].is_dir) {
        printf("purge: no such file '%s'\n", args[1].c_str());
        return;
    }
    int freed = stored_bytes(i);
    nodes[i].used = false;
    nodes[i].versions.clear();
    printf("'%s' permanently deleted (%d bytes freed, not in trash)\n", args[1].c_str(), freed);
    log_event("file purged: " + args[1]);
}

// Command "fsstat": shows how much memory the files use, compared with what it
// would be if every version was a full copy, and how many files/slots are used.
void cmd_fsstat() {
    int files = 0;
    int versions = 0;
    int stored = 0;
    int full = 0;
    int slots = 0;
    for (int i = 1; i < MAX_NODES; i++) {
        if (!nodes[i].used) {
            continue;
        }
        slots++;
        if (!nodes[i].is_dir) {
            files++;
            versions += nodes[i].versions.size();
            stored += stored_bytes(i);
            full += full_bytes(i);
        }
    }
    printf("  files: %d   versions: %d   node slots used: %d/%d\n", files, versions, slots, MAX_NODES - 1);
    printf("  stored: %d bytes   as full copies it would be: %d bytes   saved: %d bytes", stored, full, full - stored);
    if (full > 0) {
        printf(" (%d%%)", (full - stored) * 100 / full);
    }
    printf("\n  limit: %d versions per file (oldest dropped automatically)\n", MAX_VERSIONS);
}
