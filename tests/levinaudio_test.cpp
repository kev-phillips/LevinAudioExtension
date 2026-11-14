// Standalone tests for levinplayer/include/levinaudio.h
//   g++ -std=c++11 -Wall -Wextra -I levinplayer/include tests/levinaudio_test.cpp -o /tmp/levinaudio_test
//   /tmp/levinaudio_test [file.levinaudio module.xm]
#include "levinaudio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

static void put_u32(std::vector<unsigned char> &v, uint32_t x)
{
    for (int i = 0; i < 4; ++i) v.push_back((unsigned char)(x >> (8 * i)));
}

static void chunk(std::vector<unsigned char> &v, const char *id, const std::string &body)
{
    v.insert(v.end(), id, id + 4);
    put_u32(v, (uint32_t)body.size());
    v.insert(v.end(), body.begin(), body.end());
}

static std::vector<unsigned char> header(uint16_t version = 1)
{
    std::vector<unsigned char> v = { 'L', 'V', 'N', 'A', (unsigned char)(version & 0xff), (unsigned char)(version >> 8), 0, 0 };
    return v;
}

static std::vector<unsigned char> read_file(const char *path)
{
    std::vector<unsigned char> v;
    FILE *f = fopen(path, "rb");
    if (!f) return v;
    int c;
    while ((c = fgetc(f)) != EOF) v.push_back((unsigned char)c);
    fclose(f);
    return v;
}

int main(int argc, char **argv)
{
    LevinAudioFile f;

    { // meta + xm + unknown chunk
        std::vector<unsigned char> v = header();
        chunk(v, "META", "{\"title\":\"x\"}");
        chunk(v, "XM  ", "Extended Module: fake");
        chunk(v, "EDIT", "editor stuff");
        CHECK(levinaudio_parse(v.data(), (uint32_t)v.size(), &f) == 1);
        CHECK(std::string(f.module_ext) == ".xm");
        CHECK(std::string((const char *)f.module, f.module_size) == "Extended Module: fake");
        CHECK(std::string(f.meta, f.meta_size) == "{\"title\":\"x\"}");
    }
    { // mod, no meta, module first
        std::vector<unsigned char> v = header();
        chunk(v, "MOD ", "modbytes");
        CHECK(levinaudio_parse(v.data(), (uint32_t)v.size(), &f) == 1);
        CHECK(std::string(f.module_ext) == ".mod");
        CHECK(f.meta == 0 && f.meta_size == 0);
    }
    { // plain XM is not a container
        const char *xm = "Extended Module: plain";
        CHECK(levinaudio_is_container((const unsigned char *)xm, 22) == 0);
        CHECK(levinaudio_parse((const unsigned char *)xm, 22, &f) == 0);
    }
    { // no module
        std::vector<unsigned char> v = header();
        chunk(v, "META", "{}");
        CHECK(levinaudio_parse(v.data(), (uint32_t)v.size(), &f) == 0);
    }
    { // two modules
        std::vector<unsigned char> v = header();
        chunk(v, "XM  ", "a");
        chunk(v, "MOD ", "b");
        CHECK(levinaudio_parse(v.data(), (uint32_t)v.size(), &f) == 0);
    }
    { // truncated body
        std::vector<unsigned char> v = header();
        chunk(v, "XM  ", "abcdef");
        v.pop_back();
        CHECK(levinaudio_parse(v.data(), (uint32_t)v.size(), &f) == 0);
    }
    { // truncated chunk header
        std::vector<unsigned char> v = header();
        chunk(v, "XM  ", "abc");
        v.push_back('M'); v.push_back('E');
        CHECK(levinaudio_parse(v.data(), (uint32_t)v.size(), &f) == 0);
    }
    { // huge size must not overflow
        std::vector<unsigned char> v = header();
        v.insert(v.end(), { 'X', 'M', ' ', ' ' });
        put_u32(v, 0xFFFFFFFFu);
        CHECK(levinaudio_parse(v.data(), (uint32_t)v.size(), &f) == 0);
    }
    { // future version refused, version 0 refused
        std::vector<unsigned char> v = header(2);
        chunk(v, "XM  ", "a");
        CHECK(levinaudio_parse(v.data(), (uint32_t)v.size(), &f) == 0);
        std::vector<unsigned char> w = header(0);
        chunk(w, "XM  ", "a");
        CHECK(levinaudio_parse(w.data(), (uint32_t)w.size(), &f) == 0);
    }
    { // empty module chunk
        std::vector<unsigned char> v = header();
        chunk(v, "XM  ", "");
        CHECK(levinaudio_parse(v.data(), (uint32_t)v.size(), &f) == 0);
    }

    if (argc == 3)
    { // file made by tools/levinaudio.py round-trips to the original module bytes
        std::vector<unsigned char> packed = read_file(argv[1]);
        std::vector<unsigned char> original = read_file(argv[2]);
        CHECK(!packed.empty() && !original.empty());
        CHECK(levinaudio_parse(packed.data(), (uint32_t)packed.size(), &f) == 1);
        CHECK(f.module_size == original.size());
        CHECK(memcmp(f.module, original.data(), original.size()) == 0);
        CHECK(f.meta_size > 0);
    }

    printf(failures ? "%d failure(s)\n" : "all levinaudio parser tests passed\n", failures);
    return failures ? 1 : 0;
}
