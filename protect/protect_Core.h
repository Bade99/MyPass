#pragma once
#include <Windows.h>
#include "protect_sha256.h"
#include "protect_twofish.h"

static std::wstring get_general_save_folder() { //NOTE: this folder is guaranteed to exist
	static std::wstring folder; //folder stored globally, if the user decides to delete it mid execution I wish them good luck
	if (folder.empty()) {
		constexpr wchar_t application_folder[] = L"\\MyPass";
		PWSTR general_folder;
		SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &general_folder);//NOTE: I dont think this has an ansi version that isnt deprecated
		(folder = general_folder) += application_folder;
		CoTaskMemFree(general_folder);
		BOOL res = CreateDirectoryW(folder.c_str(), 0);
		runtime_assert(res || GetLastError()==ERROR_ALREADY_EXISTS,"Unable to create work folder on AppData\\Roaming");
	}
	return folder;
}

static wchar_t* convert_ascii_to_utf16(char* s, int char_cnt) {
	wchar_t* res;
	int sz_char = MultiByteToWideChar(CP_ACP, 0, (LPCCH)s, char_cnt, 0, 0);
	if (sz_char) {
		void* mem = VirtualAlloc(0, sz_char * sizeof(wchar_t), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (mem) {
			MultiByteToWideChar(CP_ACP, 0, (LPCCH)s, char_cnt, (LPWSTR)mem, sz_char);
			res = (wchar_t*)mem;
		}
	}
}

void free_convert(void* converted_string) {
	VirtualFree(converted_string, 0, MEM_RELEASE);
}

u32 next_multiple_of(u32 multiple, u32 n) {
	//TODO(fran): faster!
	u32 res;
	if (n % multiple == 0) {
		res = n == 0 ? multiple : n;
	}
	else {
		res = ((n / multiple) + 1) * multiple;
	}
	return res;
}

u32 next_multiple_of_16(u32 n) {
	u32 res = next_multiple_of(16, n);
	return res;
}

//TODO(fran): check output size
void sha256(void* input, size_t len_bytes,void* output /*32 bytes*/) { //TODO(fran): SHA3-256
	sha256_ctx context;
	__sha256_init_ctx(&context);
	__sha256_process_bytes(input,len_bytes,&context);
	__sha256_finish_ctx(&context,output);
}

//TWOFISH USAGE: set the key with twofish_setkey(), then encrypt with twofish_encrypt() or decrypt with twofish_decrypt()

//After setting the key you can and should destroy in_key, we store it internally in a more secure form for posterior encryptions and decryptions
void twofish_setkey(u32 in_key[], u32 len_bytes /*16, 24 or 32 bytes*/) {
	Assert(len_bytes == 16 || len_bytes == 24 || len_bytes == 32);
	set_key(in_key, len_bytes * 8); //for some reason it requests lenght in bits
}

//TODO(fran): check that output has correct size

//INFO: input and output size will be the same. input and output can point to the same memory
void twofish_encrypt(void* input, size_t len_bytes /*must be a multiple of 16 bytes*/, void* output) {
	Assert(len_bytes % 16 == 0);

	for (int i = 0, t = len_bytes / 16; i < t; i++) {
		encrypt(&((u32*)input)[i*4], &((u32*)output)[i * 4]);
	}
}

//INFO: input and output size will be the same. input and output can point to the same memory
void twofish_decrypt(void* input, size_t len_bytes /*must be a multiple of 16 bytes*/, void* output) {
	Assert(len_bytes % 16 == 0);

	for (int i = 0, t = len_bytes / 16; i < t; i++) {
		decrypt(&((u32*)input)[i * 4], &((u32*)output)[i * 4]);
	}
}