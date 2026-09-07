#include "base/error_handling.h"

#include "base/file_scanner.h"
#include "base/logger.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

typedef struct _PE_FILE_HEADER {
    DWORD Signature;
    IMAGE_FILE_HEADER FileHeader;
} PE_FILE_HEADER, *PPE_FILE_HEADER;

bool return_free(bool res, LPVOID FileContent)
{
    UnmapViewOfFile(FileContent);
    return res;
}

static std::string strtolower(const char* src)
{
    const auto x = strlen(src);
    std::string dest = src;
    for (size_t i = 0; i < x; i++) {
        dest[i] = (char)tolower(dest[i]);
    }
    return dest;
}

static std::string ucwords(const char* src)
{
    const auto x = strlen(src);
    std::string dest = src;
    for (size_t i = 0; i < x; i++) {
        if (isalpha(dest[i]) && (i == 0 || !isalpha(dest[i - 1]))) {
            dest[i] = (char)toupper(dest[i]);
        }
    }
    return dest;
}

}

namespace PY4GW {

bool FileScanner::CreateFromPath(const wchar_t* path, FileScanner* result)
{
    HANDLE hFile = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        Logger::Instance().LogError("CreateFileW failed, err: " + std::to_string(GetLastError()));
        return false;
    }

    LARGE_INTEGER onDiskSize;
    if (!GetFileSizeEx(hFile, &onDiskSize)) {
        Logger::Instance().LogError("GetFileSizeEx failed, err: " + std::to_string(GetLastError()));
        CloseHandle(hFile);
        return false;
    }

    size_t FileSize = static_cast<size_t>(onDiskSize.QuadPart);

    HANDLE hFileMapping = CreateFileMappingW(
        hFile,
        NULL,
        SEC_IMAGE_NO_EXECUTE | PAGE_READONLY,
        0,
        0,
        NULL);

    if (hFileMapping == NULL) {
        Logger::Instance().LogError("CreateFileMappingW failed, err: " + std::to_string(GetLastError()));
        CloseHandle(hFile);
        return false;
    }

    LPVOID FileContent = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);

    CloseHandle(hFileMapping);
    CloseHandle(hFile);

    if (FileContent == NULL) {
        Logger::Instance().LogError("MapViewOfFile failed, err: " + std::to_string(GetLastError()));
        return return_free(false, FileContent);
    }

    const uint8_t* FileBytes = (const uint8_t*)FileContent;

    IMAGE_DOS_HEADER DosHeader;
    if (FileSize < sizeof(DosHeader)) {
        Logger::Instance().LogError("Not enough bytes for the IMAGE_DOS_HEADER");
        return return_free(false, FileContent);
    }

    memcpy(&DosHeader, FileContent, sizeof(DosHeader));
    if ((DosHeader.e_magic != IMAGE_DOS_SIGNATURE) || (FileSize < (size_t)DosHeader.e_lfanew)) {
        Logger::Instance().LogError("Invalid IMAGE_DOS_HEADER");
        return return_free(false, FileContent);
    }

    if (FileSize < (DosHeader.e_lfanew + sizeof(PE_FILE_HEADER))) {
        Logger::Instance().LogError("Not enough bytes for the PE_FILE_HEADER");
        return return_free(false, FileContent);
    }

    PE_FILE_HEADER PeHeader;
    memcpy(&PeHeader, FileBytes + DosHeader.e_lfanew, sizeof(PeHeader));

    if (PeHeader.Signature != IMAGE_NT_SIGNATURE) {
        Logger::Instance().LogError("Not a PE file, invalid signature");
        return return_free(false, FileContent);
    }

    if (PeHeader.FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
        Logger::Instance().LogError("Not a 32 bits executable");
        return return_free(false, FileContent);
    }

    IMAGE_OPTIONAL_HEADER32 OptHeader;
    if (PeHeader.FileHeader.SizeOfOptionalHeader < sizeof(OptHeader)) {
        Logger::Instance().LogError("Expected optional header bytes mismatch");
        return return_free(false, FileContent);
    }

    size_t OptHeaderOffset = (size_t)DosHeader.e_lfanew + sizeof(PeHeader);
    if (FileSize < (OptHeaderOffset + sizeof(OptHeader))) {
        Logger::Instance().LogError("Not enough bytes for the optional header");
        return return_free(false, FileContent);
    }

    memcpy(&OptHeader, FileBytes + OptHeaderOffset, sizeof(OptHeader));

    if ((FileSize - OptHeaderOffset) < PeHeader.FileHeader.SizeOfOptionalHeader) {
        Logger::Instance().LogError("Not enough bytes for the section headers");
        return return_free(false, FileContent);
    }

    uint32_t SectionsSize = (uint32_t)PeHeader.FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);

    if ((FileSize - PeHeader.FileHeader.SizeOfOptionalHeader) < SectionsSize) {
        Logger::Instance().LogError("Not enough bytes for the section headers");
        return return_free(false, FileContent);
    }

    PIMAGE_SECTION_HEADER Sections = IMAGE_FIRST_SECTION((PIMAGE_NT_HEADERS32)(FileBytes + DosHeader.e_lfanew));
    size_t ImageSize = OptHeader.SizeOfImage;
    WORD SectionIdx;
    for (SectionIdx = 0; SectionIdx < PeHeader.FileHeader.NumberOfSections; ++SectionIdx) {
        PIMAGE_SECTION_HEADER Section = &Sections[SectionIdx];
        if ((ImageSize < Section->VirtualAddress) || ((ImageSize - Section->VirtualAddress) < Section->Misc.VirtualSize)) {
            Logger::Instance().LogError("Not enough bytes for a section");
            return return_free(false, FileContent);
        }
    }

    FileScanner tmp = FileScanner(FileContent, OptHeader.ImageBase, Sections, PeHeader.FileHeader.NumberOfSections);
    *result = tmp;
    tmp.FileMapping = nullptr;
    return true;
}

FileScanner::FileScanner()
    : FileScanner(nullptr, 0, nullptr, 0)
{
}

FileScanner::FileScanner(const void* FileMapping, uintptr_t ImageBase, PIMAGE_SECTION_HEADER Sections, size_t Count)
    : FileMapping(FileMapping)
    , ImageBase(ImageBase)
{
    for (size_t idx = 0; idx < Count; ++idx) {
        PIMAGE_SECTION_HEADER pSectionHdr = &Sections[idx];
        ScannerSection section = ScannerSection::Count;
        if (memcmp(pSectionHdr->Name, ".text", sizeof(".text")) == 0) {
            section = ScannerSection::Text;
        } else if (memcmp(pSectionHdr->Name, ".rdata", sizeof(".rdata")) == 0) {
            section = ScannerSection::RData;
        } else if (memcmp(pSectionHdr->Name, ".data", sizeof(".data")) == 0) {
            section = ScannerSection::Data;
        }

        if (section != ScannerSection::Count) {
            sections[static_cast<size_t>(section)].start = (uintptr_t)FileMapping + pSectionHdr->VirtualAddress;
            sections[static_cast<size_t>(section)].end = sections[static_cast<size_t>(section)].start + pSectionHdr->Misc.VirtualSize;
        }
    }
}

FileScanner::~FileScanner()
{
    if (FileMapping != NULL) {
        UnmapViewOfFile(FileMapping);
    }
}

void FileScanner::GetSectionAddressRange(ScannerSection section, uintptr_t* start, uintptr_t* end) {
    const auto& range = sections[static_cast<size_t>(section)];
    if (start)
        *start = range.start;
    if (end)
        *end = range.end;
}

uintptr_t FileScanner::FindAssertion(const char* assertion_file, const char* assertion_msg, uint32_t line_number, int offset)
{
#pragma warning( push )
#pragma warning( disable : 4838 )
#pragma warning( disable : 4242 )
#pragma warning( disable : 4244 )
#pragma warning( disable : 4365 )
    int i;
    char assertion_bytes[] = "\x68????\xBA????\xB9????";
    char assertion_mask[] = "xxxxxxxxxxxxxxx";

    char* assertion_bytes_ptr = &assertion_bytes[5];
    char* assertion_mask_ptr = &assertion_mask[5];

    offset += &assertion_mask[5] - assertion_mask_ptr;

    char assertion_message_mask[128];
    for (i = 0; assertion_msg[i]; i++) {
        assertion_message_mask[i] = 'x';
    }
    assertion_message_mask[i++] = 'x';
    assertion_message_mask[i] = 0;

    char assertion_file_mask[128];
    for (i = 0; assertion_file[i]; i++) {
        assertion_file_mask[i] = 'x';
    }
    assertion_file_mask[i++] = 'x';
    assertion_file_mask[i] = 0;

    uintptr_t start = 0;
    uintptr_t end = 0;

    FileScanner::GetSectionAddressRange(ScannerSection::RData, &start, &end);

    uint32_t assertion_message_offset = start;
    uintptr_t found = 0;
    for (;;) {
        found = FindInRange(assertion_msg, assertion_message_mask, 0, assertion_message_offset, end);
        if (!found)
            break;

        assertion_message_offset = found + 1;

        uintptr_t found_enc = (found - (uintptr_t)FileMapping) + ImageBase;
        assertion_bytes[11] = found_enc;
        assertion_bytes[12] = found_enc >> 8;
        assertion_bytes[13] = found_enc >> 16;
        assertion_bytes[14] = found_enc >> 24;

        uint32_t assertion_file_offset = start;
        for (;;) {
            found = FindInRange(assertion_file, assertion_file_mask, 0, assertion_file_offset, end);
            if (!found) {
                found = FindInRange(strtolower(assertion_file).c_str(), assertion_file_mask, 0, assertion_file_offset, end);
            }
            if (!found) {
                found = FindInRange(ucwords(assertion_file).c_str(), assertion_file_mask, 0, assertion_file_offset, end);
            }
            if (!found)
                break;

            assertion_file_offset = found + 1;

            if (((char*)found)[1] != ':') {
                found = FindInRange(":", "x", -0x1, found, found - 128);
                if (!found)
                    break;
            }

            found_enc = (found - (uintptr_t)FileMapping) + ImageBase;
            assertion_bytes[6] = found_enc;
            assertion_bytes[7] = found_enc >> 8;
            assertion_bytes[8] = found_enc >> 16;
            assertion_bytes[9] = found_enc >> 24;

            if (line_number) {
                if ((line_number & 0xff) == line_number) {
                    assertion_bytes_ptr = &assertion_bytes[3];
                    assertion_mask_ptr = &assertion_mask[3];
                    assertion_bytes[3] = 0x6a;
                    assertion_bytes[4] = line_number;
                    found = Find(assertion_bytes_ptr, assertion_mask_ptr, offset);
                    if (found)
                        return found;
                }
                assertion_bytes_ptr = &assertion_bytes[0];
                assertion_mask_ptr = &assertion_mask[0];
                assertion_bytes[0] = 0x68;
                assertion_bytes[1] = line_number;
                assertion_bytes[2] = line_number >> 8;
                assertion_bytes[3] = line_number >> 16;
                assertion_bytes[4] = line_number >> 24;
                found = Find(assertion_bytes_ptr, assertion_mask_ptr, offset);
                if (found)
                    return found;
            }
            else {
                assertion_bytes_ptr = &assertion_bytes[5];
                assertion_mask_ptr = &assertion_mask[5];
                found = Find(assertion_bytes_ptr, assertion_mask_ptr, offset);
                if (found)
                    return found;
            }
        }
    }
    return 0;
#pragma warning(pop)
}

uintptr_t FileScanner::FindInRange(const char* pattern, const char* mask, int offset, uint32_t start, uint32_t end)
{
    char first = pattern[0];
    size_t patternLength = strlen(mask ? mask : pattern);
    bool found = false;
    end -= patternLength;

    if (start > end) {
        for (uintptr_t i = start; i >= end; i--) {
            if (*(char*)i != first)
                continue;
            found = true;
            for (size_t idx = 0; idx < patternLength; idx++) {
                if ((!mask || mask[idx] == 'x') && pattern[idx] != *(char*)(i + idx)) {
                    found = false;
                    break;
                }
            }
            if (found)
                return i + offset;
        }
    }
    else {
        for (uintptr_t i = start; i < end; i++) {
            if (*(char*)i != first)
                continue;
            found = true;
            for (size_t idx = 0; idx < patternLength; idx++) {
                if ((!mask || mask[idx] == 'x') && pattern[idx] != *(char*)(i + idx)) {
                    found = false;
                    break;
                }
            }
            if (found)
                return i + offset;
        }
    }
    return NULL;
}

uintptr_t FileScanner::Find(const char* pattern, const char* mask, int offset, ScannerSection section)
{
    return FindInRange(pattern, mask, offset, (uint32_t)sections[static_cast<size_t>(section)].start, (uint32_t)sections[static_cast<size_t>(section)].end);
}

}  // namespace PY4GW
