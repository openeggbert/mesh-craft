// SYS-W9-06: writeFileAtomically()/uniqueSiblingTempPath() coverage --
// first save, overwrite-existing, Unicode path, injected writer failure,
// injected finalize failure, pre-existing leftover temp file, and two
// distinct concurrent temporary names.

#include <MeshCraft/Mc3/Mc3AtomicFileWriter.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace MeshCraft::Mc3;
namespace fs = std::filesystem;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

static std::string readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), {});
}

int main() {
    fs::path dir = fs::temp_directory_path() / "mc3_atomic_write_test";
    fs::remove_all(dir);
    fs::create_directories(dir);

    // First save: destination does not exist yet.
    {
        fs::path dest = dir / "first.txt";
        writeFileAtomically(dest, [](const fs::path& tmp) {
            std::ofstream f(tmp, std::ios::binary);
            f << "hello";
        });
        check(fs::exists(dest), "first save creates the destination");
        check(readFile(dest) == "hello", "first save writes the given content");
        check(!fs::exists(dest.string() + ".tmp"),
              "first save leaves no fixed-name .tmp sibling behind");
    }

    // Overwrite an existing destination.
    {
        fs::path dest = dir / "overwrite.txt";
        writeFileAtomically(dest, [](const fs::path& tmp) {
            std::ofstream f(tmp, std::ios::binary);
            f << "version1";
        });
        writeFileAtomically(dest, [](const fs::path& tmp) {
            std::ofstream f(tmp, std::ios::binary);
            f << "version2";
        });
        check(readFile(dest) == "version2",
              "second save replaces the first destination's content");
    }

    // Unicode path.
    {
        fs::path dest = dir / fs::path(u8"ünicode_日本.txt");
        writeFileAtomically(dest, [](const fs::path& tmp) {
            std::ofstream f(tmp, std::ios::binary);
            f << "unicode-ok";
        });
        check(fs::exists(dest) && readFile(dest) == "unicode-ok",
              "Unicode destination path round-trips");
    }

    // Injected writer failure: destination must not exist, no leftover temp.
    {
        fs::path dest = dir / "writer_fails.txt";
        bool threw = false;
        try {
            writeFileAtomically(dest, [](const fs::path&) {
                throw std::runtime_error("injected writer failure");
            });
        } catch (const std::runtime_error& e) {
            threw = true;
            check(std::string(e.what()) == "injected writer failure",
                  "a writer-stage failure propagates writeFn's own exception unchanged");
        }
        check(threw, "a writer-stage failure throws");
        check(!fs::exists(dest), "a writer-stage failure never creates the destination");
        bool leftoverTemp = false;
        for (auto& entry : fs::directory_iterator(dir))
            if (entry.path().filename().string().find("writer_fails.txt.") == 0)
                leftoverTemp = true;
        check(!leftoverTemp, "a writer-stage failure removes its temporary file");
    }

    // Injected writer failure on an existing destination: the old content
    // must survive untouched.
    {
        fs::path dest = dir / "preserve_on_failure.txt";
        writeFileAtomically(dest, [](const fs::path& tmp) {
            std::ofstream f(tmp, std::ios::binary);
            f << "original";
        });
        try {
            writeFileAtomically(dest, [](const fs::path&) {
                throw std::runtime_error("injected");
            });
        } catch (const std::runtime_error&) {}
        check(readFile(dest) == "original",
              "a writer-stage failure on an overwrite preserves the old destination");
    }

    // Injected finalize failure: make the temp path's parent directory
    // unable to supply a valid rename target by deleting the directory the
    // destination itself lives in isn't practical cross-platform, so
    // instead simulate finalize failure the portable way -- pass a
    // destination whose parent directory does not exist. The writer stage
    // itself succeeds (writeFn only touches the returned tmp path, which is
    // sibling to a nonexistent parent), so this exercises the finalize path
    // specifically.
    {
        fs::path missingDir = dir / "no_such_subdir";
        fs::path dest = missingDir / "target.txt";
        bool threw = false;
        try {
            writeFileAtomically(dest, [](const fs::path& tmp) {
                // The candidate tmp path's parent (missingDir) does not
                // exist, so opening it for write fails -- this is
                // necessarily a writer-stage failure in this primitive's
                // design (there is no way to reach the finalize stage
                // without a real temp file), which is the correct, safe
                // behavior: never attempt to finalize into a directory that
                // doesn't exist.
                std::ofstream f(tmp, std::ios::binary);
                if (!f) throw std::runtime_error("cannot open temp file");
                f << "unreachable";
            });
        } catch (const std::exception&) {
            threw = true;
        }
        check(threw, "a destination whose parent directory does not exist fails safely");
        check(!fs::exists(dest), "no destination is created when the parent directory is missing");
    }

    // AtomicFinalizeError is thrown (not the generic writer exception) when
    // finalize itself is the failing stage: replace the temp file with a
    // directory of the same name right before rename by using a destination
    // that IS a directory -- rename(file -> existing directory) fails on
    // both POSIX and Windows without ever touching the directory's contents.
    {
        fs::path dest = dir / "is_a_directory";
        fs::create_directories(dest);
        bool threw = false;
        bool rightType = false;
        try {
            writeFileAtomically(dest, [](const fs::path& tmp) {
                std::ofstream f(tmp, std::ios::binary);
                f << "content";
            });
        } catch (const AtomicFinalizeError&) {
            threw = true;
            rightType = true;
        } catch (const std::exception&) {
            threw = true;
        }
        check(threw, "renaming onto an existing directory fails");
        check(rightType, "renaming onto an existing directory throws AtomicFinalizeError specifically");
        check(fs::is_directory(dest), "the pre-existing directory destination is left untouched");
    }

    // Pre-existing leftover temp file at the exact name a naive fixed-name
    // scheme would have used must not block a save.
    {
        fs::path dest = dir / "stale_tmp.txt";
        std::ofstream stale(dest.string() + ".tmp", std::ios::binary);
        stale << "leftover from a prior crash";
        stale.close();

        writeFileAtomically(dest, [](const fs::path& tmp) {
            std::ofstream f(tmp, std::ios::binary);
            f << "fresh content";
        });
        check(readFile(dest) == "fresh content",
              "a save succeeds even when a stale '<dest>.tmp' file already exists");
    }

    // Two distinct concurrent temporary names for the same destination.
    {
        fs::path dest = dir / "concurrent.txt";
        fs::path t1 = uniqueSiblingTempPath(dest);
        fs::path t2 = uniqueSiblingTempPath(dest);
        check(t1 != t2, "two calls for the same destination produce distinct temp paths");
        check(t1.parent_path() == dest.parent_path() &&
              t2.parent_path() == dest.parent_path(),
              "generated temp paths are siblings of the destination");
    }

    fs::remove_all(dir);

    if (failures == 0) { std::cout << "All atomic-write tests passed.\n"; return 0; }
    std::cerr << failures << " atomic-write test(s) failed.\n";
    return 1;
}
