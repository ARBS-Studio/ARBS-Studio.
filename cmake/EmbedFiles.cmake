# =============================================================================
# ARBS Studio - ngulit skedaret e te dhenave (Data/*.json, UI html, logo) ne
# binarin e add-on-it, qe instalimi te jete nje skedar i vetem pa varesi.
#
# Perdorimi (script mode):
#   cmake -DOUTPUT=<file.cpp> -DROOT=<repo> -P EmbedFiles.cmake
# =============================================================================

if (NOT DEFINED OUTPUT OR NOT DEFINED ROOT)
    message (FATAL_ERROR "EmbedFiles.cmake: OUTPUT dhe ROOT duhen percaktuar")
endif ()

file (GLOB dataFiles "${ROOT}/Data/*.json")
set (extraFiles
    "${ROOT}/Src/UI/arbs_palette.html"
    "${ROOT}/Resources/logo_arbs.png"
)

set (body "")
set (table "")
set (index 0)
foreach (f IN LISTS dataFiles extraFiles)
    if (NOT EXISTS "${f}")
        message (FATAL_ERROR "EmbedFiles.cmake: mungon ${f}")
    endif ()
    get_filename_component (name "${f}" NAME)
    file (READ "${f}" hex HEX)
    string (LENGTH "${hex}" hexLen)
    math (EXPR size "${hexLen} / 2")
    string (REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," hex "${hex}")
    string (REGEX REPLACE "(0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],)" "\\1\n" hex "${hex}")
    string (APPEND body "static const unsigned char kFile${index}[] = {\n${hex}0x00\n};\n")
    string (APPEND table "    { \"${name}\", kFile${index}, ${size} },\n")
    math (EXPR index "${index} + 1")
endforeach ()

set (content "// Gjeneruar automatikisht nga cmake/EmbedFiles.cmake - mos e ndryshoni.\n")
string (APPEND content "#include \"Core/Embedded.hpp\"\n#include <cstring>\n\nnamespace {\n${body}\nstruct Entry { const char* name; const unsigned char* data; unsigned long size; };\n")
string (APPEND content "static const Entry kEntries[] = {\n${table}    { nullptr, nullptr, 0 }\n};\n}\n\n")
string (APPEND content "namespace ARBS {\nnamespace Embedded {\n\n")
string (APPEND content "bool Get (const std::string& name, std::string& out)\n{\n    for (const Entry* e = kEntries; e->name != nullptr; ++e) {\n        if (name == e->name) { out.assign (reinterpret_cast<const char*> (e->data), e->size); return true; }\n    }\n    return false;\n}\n\n")
string (APPEND content "std::vector<std::string> List ()\n{\n    std::vector<std::string> names;\n    for (const Entry* e = kEntries; e->name != nullptr; ++e) names.push_back (e->name);\n    return names;\n}\n\n}\n}\n")

# shkruaj vetem nese ka ndryshime (shmang rikompilimet e panevojshme)
if (EXISTS "${OUTPUT}")
    file (READ "${OUTPUT}" old)
    if (old STREQUAL content)
        return ()
    endif ()
endif ()
file (WRITE "${OUTPUT}" "${content}")
