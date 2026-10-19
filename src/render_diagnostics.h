#pragma once

#ifndef RENDER_DIAGNOSTICS_ENABLED
#define RENDER_DIAGNOSTICS_ENABLED 1
#endif

#include "win_sdk.h"
#include "reflection.h"

#if RENDER_DIAGNOSTICS_ENABLED
#include <ShlObj.h>
#include <cwchar>
#include <string>
#endif

namespace render_diagnostics {

enum class Mode { directwrite, directwrite_gdi };

constexpr Mode selected_mode = Mode::directwrite_gdi;

constexpr Mode mode() { return selected_mode; }
constexpr bool directwrite_gdi() { return mode() == Mode::directwrite_gdi; }

#define _foreach_render_event(op) \
	op(window_resize,) \
	op(editor_resize,) \
	op(card_resize_plan,) \
	op(card_resize_apply,) \
	op(card_move,) \
	op(layout_unchanged,) \
	op(layout_move_only,) \
	op(layout_resize,) \
	op(layout_batch_failure,) \
	op(card_size_message,) \
	op(table_size_message,) \
	op(value_size_message,) \
	op(card_backdrop_move,) \
	op(card_children_batch,) \
	op(table_children_batch,) \
	op(value_children_batch,) \
	op(text_size,) \
	op(text_paint,) \
	op(static_text,) \
	op(button_paint,) \
	op(caption_paint,) \
	op(list_resize,) \
	op(list_backbuffer,) \
	op(background,) \
	op(text_format,) \
	op(text_layout,) \
	op(dc_target,) \
	op(dc_target_create,) \
	op(dc_target_create_failure,) \
	op(dc_bind_failure,) \
	op(dc_brush,) \
	op(dc_begin_draw,) \
	op(dc_end_draw,) \
	op(dc_end_failure,) \
	op(bitmap_target,) \
	op(bitmap_target_create,) \
	op(bitmap_target_resize,) \
	op(bitmap_copy_in,) \
	op(bitmap_draw_layout,) \
	op(bitmap_draw_glyph,) \
	op(bitmap_copy_out,) \
	op(bitmap_failure,) \
	op(gdi_text,)

enum class Event {
	_foreach_render_event(_generate_enum_member)
	count,
};

#if RENDER_DIAGNOSTICS_ENABLED

struct Metric {
	unsigned long long calls = 0;
	LONGLONG ticks = 0;
	LONGLONG max_ticks = 0;
};

struct Session {
	bool active = false;
	LONGLONG start = 0;
	Metric metrics[(size_t)Event::count]{};
};

Session& session() {
	static Session current{};
	return current;
}

void begin_resize_session() {
	auto& current = session();
	current = {};
	LARGE_INTEGER counter{};
	QueryPerformanceCounter(&counter);
	current.start = counter.QuadPart;
	current.active = true;
}

const wchar_t* event_name(Event event) {
	switch (event) {
	using enum Event;
	_foreach_render_event(_string_enum_case)
	default: return L"unknown";
	}
}

void record_count(Event event) {
	if (session().active) session().metrics[(size_t)event].calls++;
}

void append_report_to_file(const std::wstring& report) {
	PWSTR roaming_folder = nullptr;
	if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roaming_folder)) || !roaming_folder) {
		OutputDebugStringW(L"[render] Could not locate Roaming AppData for the log.\n");
		return;
	}
	std::wstring folder = roaming_folder;
	CoTaskMemFree(roaming_folder);
	folder += L"\\MyPass";
	if (!CreateDirectoryW(folder.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
		OutputDebugStringW(L"[render] Could not create the MyPass log folder.\n");
		return;
	}

	int byte_count = WideCharToMultiByte(CP_UTF8, 0, report.data(), (int)report.size(), nullptr, 0, nullptr, nullptr);
	if (byte_count <= 0) return;
	std::string utf8((size_t)byte_count, '\0');
	if (!WideCharToMultiByte(CP_UTF8, 0, report.data(), (int)report.size(), utf8.data(), byte_count, nullptr, nullptr)) return;

	std::wstring path = folder + L"\\render-diagnostics.log";
	HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		OutputDebugStringW(L"[render] Could not open the diagnostics log.\n");
		return;
	}
	size_t offset = 0;
	while (offset < utf8.size()) {
		DWORD written = 0;
		if (!WriteFile(file, utf8.data() + offset, (DWORD)(utf8.size() - offset), &written, nullptr) || !written) break;
		offset += written;
	}
	CloseHandle(file);
	if (offset != utf8.size()) OutputDebugStringW(L"[render] Could not append the complete diagnostics report.\n");
}

void end_resize_session() {
	auto& current = session();
	if (!current.active) return;
	current.active = false;
	LARGE_INTEGER counter{}, frequency{};
	QueryPerformanceCounter(&counter);
	QueryPerformanceFrequency(&frequency);
	const wchar_t* mode_name = L"directwrite";
	switch (mode()) {
	case Mode::directwrite_gdi: mode_name = L"directwrite-gdi"; break;
	default: break;
	}
	SYSTEMTIME timestamp{};
	GetLocalTime(&timestamp);
	#ifdef _DEBUG
	const wchar_t* build_name = L"Debug";
	#else
	const wchar_t* build_name = L"Release";
	#endif
	wchar_t line[256]{};
	swprintf_s(line, L"[render] %04u-%02u-%02u %02u:%02u:%02u.%03u mode=%ls build=%ls resize_wall=%.2f ms\n",
		timestamp.wYear, timestamp.wMonth, timestamp.wDay, timestamp.wHour, timestamp.wMinute,
		timestamp.wSecond, timestamp.wMilliseconds, mode_name, build_name,
		1000.0 * (counter.QuadPart - current.start) / frequency.QuadPart);
	std::wstring report = line;
	for (size_t index = 0; index < (size_t)Event::count; index++) {
		const auto& metric = current.metrics[index];
		if (!metric.calls) continue;
		if (metric.ticks)
			swprintf_s(line, L"[render] %-20ls calls=%llu total=%.2f ms avg=%.3f ms max=%.3f ms\n",
				event_name((Event)index), metric.calls,
				1000.0 * metric.ticks / frequency.QuadPart,
				1000.0 * metric.ticks / frequency.QuadPart / metric.calls,
				1000.0 * metric.max_ticks / frequency.QuadPart);
		else swprintf_s(line, L"[render] %-20ls calls=%llu\n", event_name((Event)index), metric.calls);
		report += line;
	}
	report += L"\n";
	OutputDebugStringW(report.c_str());
	append_report_to_file(report);
}

struct Scope {
	Event event;
	LONGLONG start = 0;

	explicit Scope(Event event) : event(event) {
		if (session().active) {
			LARGE_INTEGER counter{};
			QueryPerformanceCounter(&counter);
			start = counter.QuadPart;
		}
	}

	~Scope() {
		if (!start) return;
		LARGE_INTEGER counter{};
		QueryPerformanceCounter(&counter);
		auto& metric = session().metrics[(size_t)event];
		LONGLONG elapsed = counter.QuadPart - start;
		metric.calls++;
		metric.ticks += elapsed;
		if (elapsed > metric.max_ticks) metric.max_ticks = elapsed;
	}
};

#else

inline void begin_resize_session() noexcept {}
inline void end_resize_session() noexcept {}
inline void record_count(Event) noexcept {}

struct Scope {
	constexpr explicit Scope(Event) noexcept {}
};

#endif

}
