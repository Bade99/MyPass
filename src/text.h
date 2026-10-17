#pragma once
#include "win_sdk.h"
#include "helpers.h"
#include "string.h"

//TODO(fran): 'show password' button (this needs to be a new element made up from 3 others: border + (editoneline + button)
//TODO(fran): ballon tips, probably handmade since windows doesnt allow it anymore, the ballon tail makes it much clearer what the tip is referring to in cases where there's many controls next to each other
//TODO(fran): paint/handle my own IME window https://docs.microsoft.com/en-us/windows/win32/intl/ime-window-class
//TODO(fran): since WM_SETTEXT doesnt notify by default we should change WM_SETTEXT_NO_NOTIFY to WM_SETTEXT_NOTIFY and reverse the current roles
//TODO(fran): IDEA for multiline with wrap around text, keep a list of wrap around points, be it a new line, word break or word that doesnt fit on its line, then we can go straight from user clicking the wnd to the corresponding line by looking how many lines does the mouse go and input that into the wrap around list to get to that line's first char idx
//TODO(fran): on WM_STYLECHANGING check for password changes, that'd need a full recalculation
//TODO(fran): on a WM_STYLECHANGING we should check if the alignment has changed and recalc/redraw every char, NOTE: I dont think windows' controls bother with this since it's not too common of a use case
//TODO(fran): at some point I got to trigger a secondary IME candidates window which for some reason decided to show up, this BUG is probably related to the multiple candidates window, maybe we need to manually block the secondary ones, it could be that if you write too much text this extra candidate windows appear
//TODO(fran): it seems like the IME window is global or smth like that, if I dont ask for candidate windows on WM_IME_SETCONTEXT then it also wont show them for other controls, wtf
//TODO(fran): change EM_SETINVALIDCHARS to be a function call provided by the user, 2 opts: bool that says tell me if there's anything invalid OR remove anything invalid, 2nd opt return array with indexes of invalid chars

namespace edit_oneline {

auto get_state(HWND wnd) { _control_create_function__get_state }

void set_user_extra(HWND wnd, void* user_extra) {
	State& state = *get_state(wnd);
	if (&state) {
		state.user_extra = user_extra;
		//TODO(fran): redraw?
	}
}

void set_functions(HWND wnd, const Functions& functions) { _control_create_function__set_functions }

SIZE calc_caret_dim(State& state) {
	SIZE res;
	HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
	HFONT oldfont = SelectFont(dc, state.theme.font); defer{ SelectFont(dc, oldfont); };
	TEXTMETRIC tm;
	GetTextMetrics(dc, &tm);
	int avg_height = tm.tmHeight;
	res.cx = 1;
	res.cy = avg_height;
	return res;
}

int calc_line_height_px(State& state) {
	int res;
	HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
	HFONT oldfont = SelectFont(dc, state.theme.font); defer{ SelectFont(dc, oldfont); };
	TEXTMETRIC tm; GetTextMetrics(dc, &tm);
	res = tm.tmHeight;
	return res;
}

template<typename T>
void release_com(T*& object) {
	if (object) {
		object->Release();
		object = nil;
	}
}

struct DirectWriteFactories {
	IDWriteFactory* write;
	ID2D1Factory* draw;
};

DirectWriteFactories& get_directwrite_factories() {
	static DirectWriteFactories factories{};
	if (!factories.write)
		DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&factories.write);
	if (!factories.draw)
		D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factories.draw);
	return factories;
}

bool is_high_surrogate(utf16 c);
bool is_low_surrogate(utf16 c);

struct DirectWriteScriptRun {
	UINT32 text_start;
	UINT32 text_length;
	DWRITE_SCRIPT_ANALYSIS analysis;
};

struct DirectWriteBidiRun {
	UINT32 text_start;
	UINT32 text_length;
	UINT8 resolved_level;
};

class DirectWriteTextAnalysis : public IDWriteTextAnalysisSource, public IDWriteTextAnalysisSink {
	LONG reference_count = 1;
	const utf16* text;
	UINT32 text_length;
	DWRITE_READING_DIRECTION reading_direction;
	utf16 locale_name[LOCALE_NAME_MAX_LENGTH]{};

public:
	std::vector<DirectWriteScriptRun> script_runs;
	std::vector<DirectWriteBidiRun> bidi_runs;

	DirectWriteTextAnalysis(const utf16* text, UINT32 text_length, DWRITE_READING_DIRECTION reading_direction) :
		text(text), text_length(text_length), reading_direction(reading_direction)
	{
		if (!GetUserDefaultLocaleName(locale_name, ARRAYSIZE(locale_name))) wcscpy_s(locale_name, _t("en-us"));
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interface_id, void** object) override {
		if (!object) return E_POINTER;
		*object = nil;
		if (interface_id == __uuidof(IUnknown) || interface_id == __uuidof(IDWriteTextAnalysisSource))
			*object = static_cast<IDWriteTextAnalysisSource*>(this);
		else if (interface_id == __uuidof(IDWriteTextAnalysisSink))
			*object = static_cast<IDWriteTextAnalysisSink*>(this);
		else return E_NOINTERFACE;
		AddRef();
		return S_OK;
	}
	ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&reference_count); }
	ULONG STDMETHODCALLTYPE Release() override {
		ULONG references = (ULONG)InterlockedDecrement(&reference_count);
		if (!references) delete this;
		return references;
	}
	HRESULT STDMETHODCALLTYPE GetTextAtPosition(UINT32 position, const WCHAR** string, UINT32* length) override {
		if (!string || !length) return E_POINTER;
		if (position >= text_length) {
			*string = nil;
			*length = 0;
		}
		else {
			*string = text + position;
			*length = text_length - position;
		}
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetTextBeforePosition(UINT32 position, const WCHAR** string, UINT32* length) override {
		if (!string || !length) return E_POINTER;
		if (!position || position > text_length) {
			*string = nil;
			*length = 0;
		}
		else {
			*string = text;
			*length = position;
		}
		return S_OK;
	}
	DWRITE_READING_DIRECTION STDMETHODCALLTYPE GetParagraphReadingDirection() override { return reading_direction; }
	HRESULT STDMETHODCALLTYPE GetLocaleName(UINT32 position, UINT32* length, const WCHAR** name) override {
		if (!length || !name) return E_POINTER;
		*length = position < text_length ? text_length - position : 0;
		*name = locale_name;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetNumberSubstitution(UINT32 position, UINT32* length, IDWriteNumberSubstitution** substitution) override {
		if (!length || !substitution) return E_POINTER;
		*length = position < text_length ? text_length - position : 0;
		*substitution = nil;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE SetScriptAnalysis(UINT32 position, UINT32 length, const DWRITE_SCRIPT_ANALYSIS* analysis) override {
		if (!analysis) return E_POINTER;
		try { script_runs.push_back({ position, length, *analysis }); }
		catch (...) { return E_OUTOFMEMORY; }
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE SetLineBreakpoints(UINT32, UINT32, const DWRITE_LINE_BREAKPOINT*) override { return S_OK; }
	HRESULT STDMETHODCALLTYPE SetBidiLevel(UINT32 position, UINT32 length, UINT8, UINT8 resolved_level) override {
		try { bidi_runs.push_back({ position, length, resolved_level }); }
		catch (...) { return E_OUTOFMEMORY; }
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE SetNumberSubstitution(UINT32, UINT32, IDWriteNumberSubstitution*) override { return S_OK; }
};

bool analyze_script_runs(const utf16* text, size_t text_length, bool right_to_left,
	std::vector<DirectWriteScriptRun>& script_runs, IDWriteTextAnalyzer1** analyzer1 = nil,
	std::vector<DirectWriteBidiRun>* bidi_runs = nil)
{
	if (analyzer1) *analyzer1 = nil;
	if (text_length > (size_t)U32MAX) return false;
	auto& factories = get_directwrite_factories();
	if (!factories.write) return false;

	IDWriteTextAnalyzer* analyzer = nil;
	if (factories.write->CreateTextAnalyzer(&analyzer) != S_OK) return false;
	defer{ release_com(analyzer); };
	IDWriteTextAnalyzer1* extended_analyzer = nil;
	if (analyzer->QueryInterface(__uuidof(IDWriteTextAnalyzer1), (void**)&extended_analyzer) != S_OK) return false;
	defer{ release_com(extended_analyzer); };

	auto analysis = new (std::nothrow) DirectWriteTextAnalysis(text, (UINT32)text_length,
		right_to_left ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
	if (!analysis) return false;
	HRESULT result = analyzer->AnalyzeScript(analysis, 0, (UINT32)text_length, analysis);
	if (result == S_OK && bidi_runs) result = analyzer->AnalyzeBidi(analysis, 0, (UINT32)text_length, analysis);
	if (result == S_OK) script_runs = std::move(analysis->script_runs);
	if (result == S_OK && bidi_runs) *bidi_runs = std::move(analysis->bidi_runs);
	analysis->Release();
	if (result != S_OK) return false;
	if (analyzer1) {
		*analyzer1 = extended_analyzer;
		extended_analyzer = nil;
	}
	return true;
}

bool script_is_neutral(UINT32 script) {
	return script == DWRITE_MAKE_OPENTYPE_TAG('Z','y','y','y') ||
		script == DWRITE_MAKE_OPENTYPE_TAG('Z','i','n','h') ||
		script == DWRITE_MAKE_OPENTYPE_TAG('Z','z','z','z');
}

bool line_is_right_to_left(const State& state, const utf16* text, size_t text_length, bool password) {
	if (password) return false;
	LONG_PTR extended_style = GetWindowLongPtr(state.wnd, GWL_EXSTYLE);
	bool layout_rtl = extended_style & WS_EX_LAYOUTRTL;
	bool reading_rtl = extended_style & WS_EX_RTLREADING;
	if (layout_rtl || reading_rtl) return layout_rtl != reading_rtl;

	std::vector<DirectWriteScriptRun> script_runs;
	std::vector<DirectWriteBidiRun> bidi_runs;
	IDWriteTextAnalyzer1* script_analyzer = nil;
	analyze_script_runs(text, text_length, false, script_runs, &script_analyzer, &bidi_runs);
	defer{ release_com(script_analyzer); };

	for (size_t i = 0; i < text_length;) {
		int code_unit_count = i + 1 < text_length && is_high_surrogate(text[i]) && is_low_surrogate(text[i + 1]) ? 2 : 1;
		WORD character_types[2]{};
		if (GetStringTypeW(CT_CTYPE2, text + i, code_unit_count, character_types)) {
			for (int code_unit = 0; code_unit < code_unit_count; code_unit++) {
				if (character_types[code_unit] == C2_LEFTTORIGHT) return false;
				if (character_types[code_unit] == C2_RIGHTTOLEFT) return true;
			}
		}
		for (const auto& run : script_runs) {
			if (i < run.text_start || i >= (size_t)run.text_start + run.text_length) continue;
			if (script_analyzer) {
				DWRITE_SCRIPT_PROPERTIES properties{};
				if (script_analyzer->GetScriptProperties(run.analysis, &properties) == S_OK) {
					if (script_is_neutral(properties.isoScriptCode)) break;
					for (const auto& bidi_run : bidi_runs) {
						if (i < bidi_run.text_start || i >= (size_t)bidi_run.text_start + bidi_run.text_length) continue;
						return (bidi_run.resolved_level & 1) != 0;
					}
					break;
				}
			}
			break;
		}
		i += code_unit_count;
	}
	return false;
}

bool create_text_format(State& state, HDC dc, IDWriteTextFormat** format) {
	*format = nil;
	auto& factories = get_directwrite_factories();
	if (!factories.write) return false;

	LOGFONT logfont{};
	if (GetObject(state.theme.font, sizeof(logfont), &logfont) != sizeof(logfont)) return false;
	TEXTMETRIC metrics{};
	GetTextMetrics(dc, &metrics);
	f32 font_size = (f32)(logfont.lfHeight < 0 ? -logfont.lfHeight : metrics.tmHeight - metrics.tmInternalLeading);
	font_size = maximum(1.f, font_size);

	utf16 locale_name[LOCALE_NAME_MAX_LENGTH]{};
	if (!GetUserDefaultLocaleName(locale_name, ARRAYSIZE(locale_name))) wcscpy_s(locale_name, _t("en-us"));
	DWRITE_FONT_WEIGHT weight = logfont.lfWeight ? (DWRITE_FONT_WEIGHT)clamp(1L, logfont.lfWeight, 999L) : DWRITE_FONT_WEIGHT_NORMAL;
	DWRITE_FONT_STYLE font_style = logfont.lfItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;
	return factories.write->CreateTextFormat(logfont.lfFaceName, nil, weight, font_style,
		DWRITE_FONT_STRETCH_NORMAL, font_size, locale_name, format) == S_OK;
}

bool create_line_text_layout(State& state, HDC dc, const TextLineLayout& line,
	IDWriteTextLayout** text_layout, str& rendered_text, bool mask_password = true)
{
	*text_layout = nil;
	if (line.text_length > (size_t)U32MAX) return false;
	auto& factories = get_directwrite_factories();
	if (!factories.write) return false;

	IDWriteTextFormat* format = nil;
	if (!create_text_format(state, dc, &format)) return false;
	defer{ release_com(format); };
	format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

	bool password = mask_password && (GetWindowLongPtr(state.wnd, GWL_STYLE) & ES_PASSWORD);
	const utf16* text = state.char_text.c_str() + line.text_start;
	if (password) {
		rendered_text.assign(line.text_length, password_char);
		text = rendered_text.c_str();
	}

	HRESULT result = factories.write->CreateGdiCompatibleTextLayout(text, (UINT32)line.text_length,
		format, 1000000.f, 1000000.f, 1.f, nil, FALSE, text_layout);
	if (result != S_OK) return false;
	(*text_layout)->SetReadingDirection(line.right_to_left ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
	(*text_layout)->SetFlowDirection(DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
	DWRITE_TEXT_METRICS metrics{};
	if ((*text_layout)->GetMetrics(&metrics) == S_OK) {
		(*text_layout)->SetMaxWidth(maximum(1.f, metrics.widthIncludingTrailingWhitespace));
		(*text_layout)->SetMaxHeight(maximum(1.f, metrics.height));
	}
	return true;
}

void set_fallback_line_boundaries(const utf16* text, TextLineLayout& line) {
	line.caret_stops.assign(line.text_length + 1, true);
	line.whitespace_characters.assign(line.text_length, false);
	for (size_t boundary = 1; boundary < line.text_length; boundary++) {
		WORD character_type = 0;
		GetStringTypeW(CT_CTYPE3, text + boundary, 1, &character_type);
		bool continuation =
			(is_high_surrogate(text[boundary - 1]) && is_low_surrogate(text[boundary])) ||
			(character_type & (C3_NONSPACING | C3_DIACRITIC | C3_VOWELMARK)) ||
			text[boundary] == 0x200d || text[boundary - 1] == 0x200d ||
			(text[boundary] >= 0xfe00 && text[boundary] <= 0xfe0f);
		line.caret_stops[boundary] = !continuation;
	}
	for (size_t character = 0; character < line.text_length; character++)
		line.whitespace_characters[character] = iswspace(text[character]) != 0;
}

//TODO(fran): DirectWrite exposes shaping clusters but not complete UAX #29 extended grapheme or word segmentation; consider intersecting ICU boundaries with these caret stops if full Unicode editor behavior becomes necessary.
void analyze_line_layout(State& state, HDC dc, TextLineLayout& line) {
	line.leading_caret_positions.assign(line.text_length + 1, 0);
	line.trailing_caret_positions.assign(line.text_length + 1, 0);
	line.caret_stops.assign(line.text_length + 1, false);
	line.whitespace_characters.assign(line.text_length, false);
	line.visual_caret_stops.clear();
	line.size = { 0, calc_line_height_px(state) };

	bool password = GetWindowLongPtr(state.wnd, GWL_STYLE) & ES_PASSWORD;
	const utf16* text = state.char_text.c_str() + line.text_start;
	line.right_to_left = line_is_right_to_left(state, text, line.text_length, password);
	if (!line.text_length) {
		line.caret_stops[0] = true;
		line.visual_caret_stops.push_back({ 0, 0, false });
		return;
	}

	str rendered_text;
	IDWriteTextLayout* text_layout = nil;
	if (create_line_text_layout(state, dc, line, &text_layout, rendered_text)) {
		defer{ release_com(text_layout); };
		DWRITE_TEXT_METRICS text_metrics{};
		if (text_layout->GetMetrics(&text_metrics) == S_OK)
			line.size.cx = (LONG)ceilf(text_metrics.widthIncludingTrailingWhitespace);

		IDWriteTextLayout* cluster_layout = text_layout;
		IDWriteTextLayout* logical_layout = nil;
		str logical_text;
		if (password && create_line_text_layout(state, dc, line, &logical_layout, logical_text, false))
			cluster_layout = logical_layout;
		defer{ release_com(logical_layout); };

		set_fallback_line_boundaries(text, line);
		std::vector<u8> cluster_boundaries(line.text_length + 1, false);
		cluster_boundaries[0] = true;
		UINT32 cluster_count = 0;
		cluster_layout->GetClusterMetrics(nil, 0, &cluster_count);
		std::vector<DWRITE_CLUSTER_METRICS> clusters(cluster_count);
		if (cluster_count && cluster_layout->GetClusterMetrics(clusters.data(), cluster_count, &cluster_count) == S_OK) {
			size_t boundary = 0;
			for (const auto& cluster : clusters) {
				for (UINT16 i = 0; i < cluster.length && boundary + i < line.text_length; i++)
					line.whitespace_characters[boundary + i] = cluster.isWhitespace;
				boundary = minimum(boundary + cluster.length, line.text_length);
				cluster_boundaries[boundary] = true;
			}

			std::vector<DirectWriteScriptRun> script_runs;
			IDWriteTextAnalyzer1* analyzer = nil;
			if (analyze_script_runs(text, line.text_length, line.right_to_left, script_runs, &analyzer)) {
				defer{ release_com(analyzer); };
				for (const auto& run : script_runs) {
					DWRITE_SCRIPT_PROPERTIES properties{};
					if (analyzer->GetScriptProperties(run.analysis, &properties) != S_OK || !properties.restrictCaretToClusters)
						continue;
					size_t run_start = minimum((size_t)run.text_start, line.text_length);
					size_t run_end = minimum((size_t)run.text_start + run.text_length, line.text_length);
					for (size_t caret = run_start + 1; caret < run_end; caret++) line.caret_stops[caret] = false;
					for (size_t caret = run_start; caret <= run_end; caret++)
						if (cluster_boundaries[caret]) line.caret_stops[caret] = true;
				}
			}
			else line.caret_stops = std::move(cluster_boundaries);
		}

		for (UINT32 boundary = 0; boundary <= (UINT32)line.text_length; boundary++) {
			f32 leading_x = 0, trailing_x = 0, ignored_y = 0;
			DWRITE_HIT_TEST_METRICS hit{};
			if (boundary < line.text_length)
				text_layout->HitTestTextPosition(boundary, FALSE, &leading_x, &ignored_y, &hit);
			else
				text_layout->HitTestTextPosition(boundary - 1, TRUE, &leading_x, &ignored_y, &hit);
			if (boundary)
				text_layout->HitTestTextPosition(boundary - 1, TRUE, &trailing_x, &ignored_y, &hit);
			else
				text_layout->HitTestTextPosition(0, FALSE, &trailing_x, &ignored_y, &hit);
			line.leading_caret_positions[boundary] = leading_x;
			line.trailing_caret_positions[boundary] = trailing_x;
		}
	}
	else {
		set_fallback_line_boundaries(text, line);
		str password_text;
		if (password) {
			password_text.assign(line.text_length, password_char);
			text = password_text.c_str();
		}
		std::vector<int> partial_extents(line.text_length);
		int fit = 0;
		GetTextExtentExPoint(dc, text, (int)line.text_length, I32MAX, &fit, partial_extents.data(), &line.size);
		if (line.right_to_left) line.leading_caret_positions[0] = line.trailing_caret_positions[0] = (f32)line.size.cx;
		for (size_t boundary = 1; boundary <= line.text_length; boundary++) {
			f32 position = line.right_to_left ? (f32)(line.size.cx - partial_extents[boundary - 1]) : (f32)partial_extents[boundary - 1];
			line.leading_caret_positions[boundary] = position;
			line.trailing_caret_positions[boundary] = position;
		}
	}

	for (size_t boundary = 0; boundary <= line.text_length; boundary++) {
		if (!line.caret_stops[boundary]) continue;
		f32 leading_x = line.leading_caret_positions[boundary];
		f32 trailing_x = line.trailing_caret_positions[boundary];
		line.visual_caret_stops.push_back({ boundary, leading_x, false });
		if (trailing_x != leading_x) line.visual_caret_stops.push_back({ boundary, trailing_x, true });
	}
	std::sort(line.visual_caret_stops.begin(), line.visual_caret_stops.end(),
		[](const TextCaretStop& a, const TextCaretStop& b) {
			if (a.x != b.x) return a.x < b.x;
			if (a.character_index != b.character_index) return a.character_index < b.character_index;
			return a.trailing < b.trailing;
		});
}

void rebuild_text_layout(State& state) {
	state.line_layouts.clear();
	HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
	HFONT oldfont = SelectFont(dc, state.theme.font); defer{ SelectFont(dc, oldfont); };
	size_t line_start = 0;
	for (size_t line_index = 0; line_index <= state.line_breaks.size(); line_index++) {
		size_t line_end = line_index < state.line_breaks.size() ? state.line_breaks[line_index] : state.char_text.size();
		TextLineLayout line{ .text_start = line_start, .text_length = line_end - line_start };
		analyze_line_layout(state, dc, line);
		state.line_layouts.push_back(std::move(line));
		line_start = line_end + 1;
	}
}

const TextLineLayout* get_line_layout(const State& state, size_t line_index) {
	return line_index < state.line_layouts.size() ? &state.line_layouts[line_index] : nil;
}

f32 line_caret_x(const TextLineLayout& line, size_t local_character_index, bool trailing = true) {
	local_character_index = minimum(local_character_index, line.text_length);
	const auto& positions = trailing ? line.trailing_caret_positions : line.leading_caret_positions;
	return positions.empty() ? 0 : positions[local_character_index];
}

int line_render_x(const State& state, const TextLineLayout& line) {
	RECT rc; GetClientRect(state.wnd, &rc);
	LONG_PTR style = GetWindowLongPtr(state.wnd, GWL_STYLE);
	if (style & ES_CENTER) return rc.left + (RECTW(rc) - line.size.cx) / 2;
	if (style & ES_RIGHT) return rc.right - state.padding.x - line.size.cx;
	return rc.left + state.padding.x;
}

SIZE calc_text_dim(State& state) {
	SIZE result{ 0, calc_line_height_px(state) * (LONG)maximum((size_t)1, state.line_layouts.size()) };
	for (auto& line : state.line_layouts) result.cx = maximum(result.cx, line.size.cx);
	return result;
}

//"Hello\n\n\n"
// -lines: "Hello\n" "\n "\n"
// -cursor idx mapping: func(char_idx = 1 aka H|ello) -> line_char_idx = 0 aka |Hello ; line_idx = 0 aka Hello|\n
//						func(char_idx = 6 aka Hello\n|\n) -> line_char_idx = 6 Hello\n|\n ; line_idx = 1 aka Hello\n|\n

//Finds out the index of the first character of the line where the input character index belongs to
//(This will be either 0 for the first line, or one character past the \n of the previous line for all other lines)
auto char_idx_to_line_char_idx(const State& state, size_t char_idx) {
	struct CharIndexToLineCharacterIndexResult { u64 line_char_idx; u64 line_idx; } res;
	res.line_char_idx = state.char_text.rfind('\n', safe_subtract0(char_idx, 1)) + 1; //TODO(fran): I do this instead of iterating over state.line_breaks cause I think it scales better to larger strings and may even be generally faster or close to it, though there maybe be a fast way to search through state.line_breaks, otherwise having that cache is pointless & I should remove it

	res.line_idx = 0;
	if (res.line_char_idx) {
		auto line_break = std::lower_bound(state.line_breaks.begin(), state.line_breaks.end(), res.line_char_idx - 1);
		if (line_break != state.line_breaks.end() && *line_break == res.line_char_idx - 1) {
			u64 line_idx = line_break - state.line_breaks.begin();
			if (char_idx == *line_break) res.line_idx = line_idx;
			else res.line_idx = line_idx + 1;
		}
	}
	//TODO(fran): now I need to know the line idx, I guess the cache wasnt so pointless after all, although this search pattern may even be faster if I now add a hash table to store a mapping line_char_idx -> line_idx (remember to remove the -1 before using it on the hash table though)
		//NOTE: I will usually need both line_idx and line_char_idx, since I use the first one to move in Y and the other to move in X
		//TODO(fran)?: if we dont want to do the hash table thing we could estimate the index: line_idx_estimation = (char_idx / total_chars) * total_lines; and iterate forward or backwards from there

	//TODO(fran): im having this issue with the fact that line_idx = 0 is not always a valid index into state.line_breaks, and we have a dual mapping for 0, it will return 0 when there are no line breaks, but also 0 when we are on the first line break. The code needs to be re-tought, maybe we should return a line count, that is always at least 1. This would fix the dual mapping problem though it would still not fix the access into state.line_breaks
	return res;
}

POINT calc_caret_p(const State& state) {
	auto [line_char_idx, line_idx] = char_idx_to_line_char_idx(state, state.selection.cursor);
	const TextLineLayout* line = get_line_layout(state, line_idx);
	POINT res = {
		line ? line_render_x(state, *line) + (LONG)lroundf(line_caret_x(*line, state.selection.cursor - line_char_idx, state.caret.trailing)) : state.padding.x,
		state.padding.y,
	};

	HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
	HFONT oldfont = SelectFont(dc, state.theme.font); defer{ SelectFont(dc, oldfont); };
	TEXTMETRIC tm; GetTextMetrics(dc, &tm);

	res.y += (LONG)(line_idx * tm.tmHeight);
	return res;
}

i32 min_height(State& state) {
	i32 res = state.padding.y + calc_line_height_px(state);
	return res;
}

i32 desired_height(State& state) {
	i32 res = state.padding.y + calc_line_height_px(state) * ((i32)state.line_breaks.size() + 1);
	return res;
}

//TODO(fran): positioning relative to a character offset, eg {3,5} 3rd char of 5th row place on top or left or...
void show_tip(HWND wnd, const cstr* msg, int duration_ms, u32 ETP_flags) {
	State& state = *get_state(wnd); Assert(&state);
	TOOLINFO toolInfo{ TTTOOLINFO_V1_SIZE };
	toolInfo.hwnd = state.wnd;
	toolInfo.uId = (UINT_PTR)state.wnd;
	toolInfo.hinst = GetModuleHandle(NULL);
	toolInfo.lpszText = const_cast<cstr*>(msg);
	SendMessage(state.controls.tooltip, TTM_UPDATETIPTEXT, 0, (LPARAM)&toolInfo);
	SendMessage(state.controls.tooltip, TTM_TRACKACTIVATE, TRUE, (LPARAM)&toolInfo);
	POINT tooltip_p{ 0,0 };
	RECT tooltip_r;
	tooltip_r.left = 0;
	tooltip_r.right = 50;
	tooltip_r.top = 0;
	tooltip_r.bottom = calc_line_height_px(state);//The tooltip uses the same font, so we know the height of its text area
	SendMessage(state.controls.tooltip, TTM_ADJUSTRECT, TRUE, (LPARAM)&tooltip_r);//convert text area to tooltip full area

	RECT rc; GetClientRect(state.wnd, &rc);

	if (ETP_flags & ETP::left) 
		tooltip_p.x = 0;

	if (ETP_flags & ETP::right) 
		tooltip_p.x = RECTW(rc);

	if (ETP_flags & ETP::top) 
		tooltip_p.y = -RECTH(tooltip_r);

	if (ETP_flags & ETP::bottom) 
		tooltip_p.y = RECTH(rc);

	ClientToScreen(state.wnd, &tooltip_p);
	SendMessage(state.controls.tooltip, TTM_TRACKPOSITION, 0, (LPARAM)MAKELONG(tooltip_p.x, tooltip_p.y));
	SetTimer(state.wnd, EDITONELINE_tooltip_timer_id, duration_ms, nil);
}

void scroll_to_caret(State& state) {
	//TODO(fran): test right and center text alignments, @BUG: center alignment is wrong, probably due to my stupid use of padding.x, it should simply indicate the render area, instead Im using it to align the text when that should require a new variable

	RECT rc; GetClientRect(state.wnd, &rc);
	auto w = RECTW(rc), h = RECTH(rc);
	if (!w || !h) return; //TODO(fran): this is probably more accurate by checking for element visibility or smth like that
	//TODO(fran): im sure this can be done branchless
	if (int scroll = (state.caret.pos.x + state.caret.dim.cx) - (w - state.padding.x + state.scroll.x); scroll > 0) {
		//scroll right
		state.scroll.x += scroll;
		//ScrollWindowEx(state.wnd, scroll, 0, 0, 0, 0, 0, SW_ERASE|SW_INVALIDATE);
		ask_window_for_repaint(state.wnd); //TODO(fran): currently we simply invalidate the whole window and redraw everything, this may or may not be optimal
	}
	else if (int scroll = (state.caret.pos.x) - (state.scroll.x + state.padding.x); scroll < 0) {
		//scroll left
		state.scroll.x += scroll;
		ask_window_for_repaint(state.wnd);
	}

	if (int scroll = (state.caret.pos.y + state.caret.dim.cy) - (h - state.padding.y + state.scroll.y); scroll > 0) {
		//scroll down
		state.scroll.y += scroll;
		ask_window_for_repaint(state.wnd);
	}
	else if (int scroll = (state.caret.pos.y) - (state.scroll.y + state.padding.y); scroll < 0) {
		//scroll up
		state.scroll.y += scroll;
		ask_window_for_repaint(state.wnd);
	}
	else if ((GetWindowLongPtr(state.wnd, GWL_STYLE) & ES_EXPANSIBLE) && h >= desired_height(state)) {
		//HACK?: scroll up if you can fit all lines
		state.scroll.y = 0; //TODO(fran): im sure this special case will introduce bugs
		ask_window_for_repaint(state.wnd);
	}
}

void reposition_caret(State& state, bool nocheck = false) {
	POINT oldcaretp = state.caret.pos;
	state.caret.pos = calc_caret_p(state);
	if (oldcaretp != state.caret.pos || nocheck) {
		scroll_to_caret(state);

		//I havent found a way to apply a translation to the caret so that SetCaretPos automatically places it on the correct character even when the characters are scrolled, therefore we gotta apply the transformation manually
		POINT p = state.caret.pos;
		p.x -= state.scroll.x;
		p.y -= state.scroll.y;
		if (GetFocus() == state.wnd) SetCaretPos(p);
	}
	//if(GetFocus() == state.wnd) SetCaretPos(state.caret.pos); //NOTE: this introduced a bug with settext where calling settext with a null string on the focussed editbox would place the caret on the wrong place
}

void update_char_pad(State& state) {
	LONG_PTR style = GetWindowLongPtr(state.wnd, GWL_STYLE);
	RECT rc; GetClientRect(state.wnd, &rc);
	HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
	HFONT oldfont = SelectFont(dc, state.theme.font); defer{ SelectFont(dc, oldfont); };
	TEXTMETRIC tm; GetTextMetrics(dc, &tm);
	if (style & ES_CENTER) {
		//Recalc pad_x
		state.padding.x = (RECTW(rc) - calc_text_dim(state).cx) / 2;
	}
	else {
		state.padding.x = 3; //TODO(fran): user defined and 'dpi pixels' based
	}
	state.padding.y = !state.line_breaks.size() ? (rc.bottom + rc.top - tm.tmHeight) / 2 : rc.top + (RECTH(rc) % tm.tmHeight) / 2; //TODO(fran): provide additional styles so the user can change vertical alignment
}

void recalculate_text_layout(State& state) {
	rebuild_text_layout(state);
	update_char_pad(state);
	reposition_caret(state);
}

void set_theme(HWND wnd, const Theme& src) {
	auto old_font = GetWindowFont(wnd);
	_control_create_function__set_theme;
	if (&state && old_font != state.theme.font) recalculate_text_layout(state);
}

void recalculate_line_breaks(State& state) {
	//TODO(fran): @speed: provide additional parameters to allow us to only recalculate line breaks after the changed characters
	state.line_breaks.clear();
	size_t linebreak = 0;
	while ((linebreak = state.char_text.find('\n', linebreak)) != str::npos) {
		state.line_breaks.push_back(linebreak);
		linebreak++;
	}
}

size_t line_index_for_character(const std::vector<size_t>& line_breaks, size_t character_index) {
	return (size_t)(std::lower_bound(line_breaks.begin(), line_breaks.end(), character_index) - line_breaks.begin());
}

TextLineLayout make_line_layout(State& state, HDC dc, size_t line_index) {
	size_t line_start = line_index ? state.line_breaks[line_index - 1] + 1 : 0;
	size_t line_end = line_index < state.line_breaks.size() ? state.line_breaks[line_index] : state.char_text.size();
	TextLineLayout line{ .text_start = line_start, .text_length = line_end - line_start };
	analyze_line_layout(state, dc, line);
	return line;
}

void replace_text_and_update_layout(State& state, size_t x_min, size_t x_max, utf16_str replacement) {
	size_t old_line_count = state.line_breaks.size() + 1;
	bool can_update_incrementally = state.line_layouts.size() == old_line_count;
	size_t old_first_line = line_index_for_character(state.line_breaks, x_min);
	size_t old_last_line = line_index_for_character(state.line_breaks, x_max);

	state.char_text.replace(x_min, x_max - x_min, replacement.str, replacement.cnt());
	recalculate_line_breaks(state);
	if (!can_update_incrementally) {
		rebuild_text_layout(state);
		return;
	}

	size_t new_first_line = line_index_for_character(state.line_breaks, x_min);
	size_t new_last_line = line_index_for_character(state.line_breaks, x_min + replacement.cnt());
	if (new_first_line != old_first_line || old_last_line >= state.line_layouts.size()) {
		rebuild_text_layout(state);
		return;
	}

	state.line_layouts.erase(state.line_layouts.begin() + old_first_line, state.line_layouts.begin() + old_last_line + 1);
	HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
	HFONT oldfont = SelectFont(dc, state.theme.font); defer{ SelectFont(dc, oldfont); };
	for (size_t line_index = new_first_line; line_index <= new_last_line; line_index++) {
		auto line = make_line_layout(state, dc, line_index);
		state.line_layouts.insert(state.line_layouts.begin() + line_index, std::move(line));
	}
	for (size_t line_index = new_last_line + 1; line_index < state.line_layouts.size(); line_index++) {
		size_t line_start = state.line_breaks[line_index - 1] + 1;
		size_t line_end = line_index < state.line_breaks.size() ? state.line_breaks[line_index] : state.char_text.size();
		state.line_layouts[line_index].text_start = line_start;
		state.line_layouts[line_index].text_length = line_end - line_start;
	}
	if (state.line_layouts.size() != state.line_breaks.size() + 1) rebuild_text_layout(state);
}

//Any event that is not a Arrow Up/Down event (keyboard strokes, mouse clicks; font/style changes?) should reset the vertical selection's stored width
void reset_vertical_selection_stored_width(State& state) {
	state.vertical_selection_stored_width = 0;
}

void check_expansibility(State& state) {
	RECT rc; GetClientRect(state.wnd, &rc);

	if (RECTH(rc) < desired_height(state))
		ask_window_for_resize(state.parent);
}

void notify_parent(State& state, WORD notif_code) {
	if (notif_code == EN_CHANGE && state.functions.on_change) state.functions.on_change(state.user_extra, state.wnd);
	else PostMessage(state.parent, WM_COMMAND, MAKELONG(state.identifier, notif_code), (LPARAM)state.wnd);
}

CharSel make_selection(State& state, CharSel::type anchor, CharSel::type cursor) {
	if (anchor == -1) {
		//Remove current selection
		anchor = cursor = state.selection.cursor; //TODO(fran): not sure if we want this functionality for this helper function, we could just set anchor = cursor = 0
	}
	else {
		anchor = clamp<CharSel::type>(0, anchor, state.char_text.length());
		cursor = clamp<CharSel::type>(0, cursor, state.char_text.length());
	}

	return CharSel{ anchor, cursor };
}

template<bool AffectHistory = false>
void set_selection(State& state, CharSel::type anchor, CharSel::type cursor, bool cursor_trailing = true) {
	size_t _start = anchor;
	size_t _end = cursor;

	if (_start == (size_t)-1) {
		//Remove current selection
		if (state.selection.anchor != state.selection.cursor) {
			state.selection.anchor = state.selection.cursor;
			ask_window_for_repaint(state.wnd);
		}
	}
	else {
		size_t end_max = (size_t)state.char_text.length();
		if (_end == (size_t)-1) {
			_end = end_max; //Set _end to one past the last valid char
		}

		_start = clamp((size_t)0, _start, end_max);
		_end = clamp((size_t)0, _end, end_max);


		if (state.selection.anchor != _start || state.selection.cursor != _end || state.caret.trailing != cursor_trailing) {
			state.selection.anchor = _start;
			state.selection.cursor = _end;
			state.caret.trailing = cursor_trailing;
			ask_window_for_repaint(state.wnd);
		}
	}

	reposition_caret(state);

	if constexpr (AffectHistory) state.history_helper.break_group(); //While an edit is hapenning we dont want to break_group, we only do this when the user's action was to actually change the selection (arrow keys, home, end, etc)
}

template<bool AffectHistory = false> void set_selection(State& state, CharSel selection) { set_selection<AffectHistory>(state, selection.anchor, selection.cursor); };

void select_all(State& state) { set_selection<true>(state, 0, -1); } //TODO(fran): consider placing inside a ::actions namespace, functions inside that namespace always affect history, to make it clearer which functions do and which dont

size_t closest_caret_stop(const State& state, size_t character_index);

void push_to_history(State& state, TextEditEntry& history_entry) { //TODO(fran): is the type right with & ? we want to take ownership of the object
	if (state.history_helper.can_merge(history_entry.selection_before)) {
		if (!state.history.update_latest([&history_entry](TextEditEntry& prior) -> size_t {
			Assert(prior.edit_kind == history_entry.edit_kind);

			switch (prior.edit_kind) {
			case TextEditEntry::Kind::typing:
			{
				Assert(history_entry.position_x_min == prior.position_x_min + prior.inserted_text.size());
				prior.inserted_text += history_entry.inserted_text;
			} break;
			case TextEditEntry::Kind::backspace:
			{
				Assert(history_entry.position_x_min + history_entry.removed_text.size() == prior.position_x_min);
				prior.position_x_min = history_entry.position_x_min;
				prior.removed_text.insert(0, history_entry.removed_text);
			} break;
			case TextEditEntry::Kind::delete_forward:
			{
				Assert(history_entry.position_x_min == prior.position_x_min);
				prior.removed_text += history_entry.removed_text;
			} break;
			default:
				Assert(0);
				break;
			}

			// Keep selection_before from the first operation in the group.
			prior.selection_after = history_entry.selection_after;
			prior.selection_after_trailing = history_entry.selection_after_trailing;

			return prior.memory_size();
			})
		)
			goto push_history;
	}
	else {
	push_history:
		state.history.push(history_entry, history_entry.memory_size()); //TODO(fran): template undo_history: if the entry type can do .memory_size() then simplify this function call to a single parameter and internally call .memory_size()
	}
	state.history_helper.record_edit(history_entry.edit_kind, history_entry.selection_after);
}

template<bool AffectHistory = true>
bool remove_selection(State& state, size_t x_min, size_t x_max) {
	if (!state.char_text.empty()) {
		auto selection_before = state.selection;
		bool selection_before_trailing = state.caret.trailing;

		x_min = clamp((size_t)0, x_min, state.char_text.length());
		x_max = clamp((size_t)0, x_max, state.char_text.length());
		if (x_min == x_max) return false;
		size_t x_cnt = distance(x_min, x_max);

		str removed_text;
		if constexpr (AffectHistory) removed_text = str(state.char_text.c_str() + x_min, x_cnt);

		utf16 empty_text[] = { 0 };
		replace_text_and_update_layout(state, x_min, x_max, to_utf_str(empty_text));

		update_char_pad(state);

		size_t cursor = closest_caret_stop(state, x_min);
		bool cursor_was_snapped = cursor != x_min;
		if constexpr (AffectHistory) {
			if (cursor_was_snapped) state.history_helper.break_group();
		}
		set_selection(state, cursor, cursor);

		reset_vertical_selection_stored_width(state); //NOTE(fran): this may not be necessary here since most events that remove selections already reset this as a side effect, but we'll do it anyways for completion and because it costs nothing

		if constexpr (AffectHistory) {
			TextEditEntry history_entry{
				.edit_kind = state.history_helper.transient_kind,
				.position_x_min = x_min,
				.removed_text = removed_text,
				.inserted_text = str(),
				.selection_before = selection_before,
				.selection_after = state.selection,
				.selection_before_trailing = selection_before_trailing,
				.selection_after_trailing = state.caret.trailing,
			};
			push_to_history(state, history_entry);
			if (cursor_was_snapped) state.history_helper.break_group();
		}
		return true;
	}
	return false;
}
template<bool AffectHistory = true> bool remove_selection(State& state, CharSel selection) { return remove_selection<AffectHistory>(state, selection.x_min(), selection.x_max()); }

//Removes the current text selection and updates the selection values
template<bool AffectHistory = true> bool remove_selection(State& state) { return remove_selection<AffectHistory>(state, state.selection.x_min(), state.selection.x_max()); }


//true if text modified, false otherwise
template<bool AffectHistory = true>
bool insert_character(State& state, utf16_str s, size_t x_min, size_t x_max) {
	auto selection_before = state.selection;
	bool selection_before_trailing = state.caret.trailing;

	//TODO(fran): center alignment get offset wrongly when inserting text of any length
	bool res = false;
	const CharSel sel{ clamp((size_t)0, x_min, state.char_text.length()), clamp((size_t)0, x_max, state.char_text.length()) };

	u64 text_length_after_removal = safe_subtract0(state.char_text.length(), sel.sel_width());
	if (text_length_after_removal <= state.char_max_sz && s.cnt() <= state.char_max_sz - text_length_after_removal) {

		//check for invalid characters
		if (state.functions.has_invalid_chars) {//TODO(fran): +1 for having always valid function pointers, we could branch on valid on invalid instead we gotta hack in a return statement in the middle of the code
			auto [invalid, explanation] = state.functions.has_invalid_chars(s.str, maximum((size_t)0, s.cnt()), state.user_extra);
			if (invalid) {
				res = false;
				show_tip(state.wnd, explanation.c_str(), EDITONELINE_default_tooltip_duration, ETP::left | ETP::top);
				return res;//HACK
			}
		}

		res = true;

		str removed_text;
		if (sel.has_selection()) {
			if constexpr (AffectHistory) removed_text = str(state.char_text.c_str() + sel.x_min(), sel.sel_width());
		}

		str inserted_text;
		if constexpr (AffectHistory) inserted_text = str(s.str, s.cnt()); //TODO(fran): unnecessary copy: template this function to allow s to be passed in and transfer ownership of the memory, if s's memory ownsership cannot be transferred only then do a copy
		replace_text_and_update_layout(state, sel.x_min(), sel.x_max(), s);

		size_t intended_cursor = sel.x_min() + s.cnt();
		size_t anchor, cursor; anchor = cursor = closest_caret_stop(state, intended_cursor);
		bool cursor_was_snapped = cursor != intended_cursor;
		if constexpr (AffectHistory) {
			if (cursor_was_snapped) state.history_helper.break_group();
		}

		set_selection(state, anchor, cursor);

		reset_vertical_selection_stored_width(state);

		update_char_pad(state);
		reposition_caret(state);

		if constexpr (AffectHistory) {
			TextEditEntry history_entry{
				.edit_kind = state.history_helper.transient_kind,
				.position_x_min = sel.x_min(),
				.removed_text = removed_text,
				.inserted_text = inserted_text,
				.selection_before = selection_before,
				.selection_after = state.selection,
				.selection_before_trailing = selection_before_trailing,
				.selection_after_trailing = state.caret.trailing,
			};
			push_to_history(state, history_entry);
			if (cursor_was_snapped) state.history_helper.break_group();
		}

		LONG_PTR style = GetWindowLongPtr(state.wnd, GWL_STYLE);
		if (style & ES_EXPANSIBLE) check_expansibility(state);
	}
	return res;
}

//inserts character replacing the current selection
template<bool AffectHistory = true>
bool insert_character(State& state, utf16 c) {//TODO(fran): optimized version for single character insertion
	utf16 txt[2];
	txt[0] = c;
	txt[1] = 0;

	return insert_character<AffectHistory>(state, to_utf_str(txt), state.selection.x_min(), state.selection.x_max());
}

//inserts string replacing the current selection
template<bool AffectHistory = true>
bool insert_character(State& state, const utf16* s) {
	bool res = false;
	if (s)
		res = insert_character<AffectHistory>(state, to_utf_str(const_cast<utf16*>(s)), state.selection.x_min(), state.selection.x_max());
	return res;
}

void begin_IME_composition(State& state) {
	auto& composition = state.ime_composition;
	Assert(!composition.active);

	composition.active = true;
	composition.document_replaced = false;
	composition.has_result = false;
	composition.position_x_min = state.selection.x_min();
	composition.selection_before = state.selection;
	composition.selection_before_trailing = state.caret.trailing;
	composition.committed_text.clear();
	composition.provisional_text.clear();

	if (state.selection.has_selection())
		composition.removed_text.assign(state.char_text, state.selection.x_min(), state.selection.sel_width());
	else
		composition.removed_text.clear();

	state.history_helper.break_group();
}

bool get_IME_string(HIMC imc, DWORD index, str& text) {
	LONG byte_count = ImmGetCompositionString(imc, index, nil, 0);
	if (byte_count < 0 || byte_count % sizeof(utf16))
		return false;

	text.resize(byte_count / sizeof(utf16));
	if (!byte_count)
		return true;

	LONG written_byte_count = ImmGetCompositionString(imc, index, text.data(), byte_count);
	if (written_byte_count < 0 || written_byte_count % sizeof(utf16))
		return false;

	text.resize(written_byte_count / sizeof(utf16));
	return true;
}

bool replace_IME_provisional_text(State& state, str& text) {
	auto& composition = state.ime_composition;
	Assert(composition.active);

	size_t x_min = composition.position_x_min + composition.committed_text.length();
	size_t x_max = composition.document_replaced ?
		x_min + composition.provisional_text.length() :
		composition.position_x_min + composition.removed_text.length();

	if (!insert_character<false>(state, to_utf_str(text), x_min, x_max))
		return false;

	composition.document_replaced = true;
	composition.provisional_text = text;
	set_selection(state, closest_caret_stop(state, x_min), closest_caret_stop(state, x_min + text.length()));
	return true;
}

bool commit_IME_result(State& state, str& result) {
	auto& composition = state.ime_composition;
	Assert(composition.active);

	size_t x_min = composition.position_x_min + composition.committed_text.length();
	size_t x_max = composition.document_replaced ?
		x_min + composition.provisional_text.length() :
		composition.position_x_min + composition.removed_text.length();

	if (!insert_character<false>(state, to_utf_str(result), x_min, x_max))
		return false;

	composition.document_replaced = true;
	composition.has_result = true;
	composition.committed_text += result;
	composition.provisional_text.clear();
	size_t cursor = closest_caret_stop(state, x_min + result.length());
	set_selection(state, cursor, cursor);
	return true;
}

bool end_IME_composition(State& state) {
	auto& composition = state.ime_composition;
	if (!composition.active)
		return false;

	bool committed_change = false;
	if (composition.has_result) {
		if (!composition.provisional_text.empty()) {
			size_t x_min = composition.position_x_min + composition.committed_text.length();
			str empty_text;
			if (!insert_character<false>(state, to_utf_str(empty_text), x_min, x_min + composition.provisional_text.length()))
				Assert(0);
			composition.provisional_text.clear();
		}

		size_t cursor = closest_caret_stop(state, composition.position_x_min + composition.committed_text.length());
		set_selection(state, cursor, cursor);

		if (composition.removed_text != composition.committed_text) {
			TextEditEntry history_entry{
				.edit_kind = TextEditEntry::Kind::ime,
				.position_x_min = composition.position_x_min,
				.removed_text = composition.removed_text,
				.inserted_text = composition.committed_text,
				.selection_before = composition.selection_before,
				.selection_after = state.selection,
				.selection_before_trailing = composition.selection_before_trailing,
				.selection_after_trailing = state.caret.trailing,
			};
			state.history_helper.record_transient_edit(TextEditEntry::Kind::ime);
			push_to_history(state, history_entry);
			committed_change = true;
		}
	}
	else if (composition.document_replaced) {
		size_t inserted_length = composition.committed_text.length() + composition.provisional_text.length();
		if (!insert_character<false>(state, to_utf_str(composition.removed_text),
			composition.position_x_min, composition.position_x_min + inserted_length))
			Assert(0);
		set_selection(state, composition.selection_before.anchor, composition.selection_before.cursor,
			composition.selection_before_trailing);
	}

	composition.active = false;
	composition.document_replaced = false;
	composition.has_result = false;
	composition.removed_text.clear();
	composition.committed_text.clear();
	composition.provisional_text.clear();
	state.ignore_IME_candidates = false;
	state.history_helper.break_group();
	return committed_change;
}

bool copy_selection(State& state, CharSel selection) {
	//Copy text from current selection to clipboard
	if (!selection.has_selection() || !OpenClipboard(state.wnd)) return false;
	defer{ CloseClipboard(); };

	HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (selection.sel_width() + 1) * sizeof(state.char_text[0]));
	if (!mem) return false;

	void* txt = GlobalLock(mem);
	if (!txt) {
		GlobalFree(mem);
		return false;
	}

	memcpy(txt, state.char_text.c_str() + selection.x_min(), selection.sel_width() * sizeof(state.char_text[0]));//copy selection
	((decltype(&state.char_text[0]))txt)[selection.sel_width()] = 0;//null terminate
	GlobalUnlock(mem);

	if (!EmptyClipboard() || !SetClipboardData(clipboard_format, mem)) {
		GlobalFree(mem);
		return false;
	}

	state.history_helper.break_group();
	return true;
}
bool copy_selection(State& state) { return copy_selection(state, state.selection); }

bool is_high_surrogate(utf16 c) { return c >= 0xd800 && c <= 0xdbff; }
bool is_low_surrogate(utf16 c) { return c >= 0xdc00 && c <= 0xdfff; }

str sanitize_pasted_text(str_view dirty_text, size_t max_char_count, bool multiline) {
	str text;
	text.reserve(minimum(dirty_text.size(), max_char_count));

	auto append_character = [&](utf16 c) {
		if (text.size() >= max_char_count) return false;
		text.push_back(c);
		return true;
	};
	auto append_line_break = [&]() { return append_character(multiline ? _t('\n') : _t(' ')); };

	for (size_t i = 0; i < dirty_text.size();) {
		utf16 c = dirty_text[i++];
		if (!c) break;

		if (c == _t('\r')) {
			if (i < dirty_text.size() && dirty_text[i] == _t('\n')) i++;
			if (!append_line_break()) break;
		}
		else if (c == _t('\n') || c == _t('\v') || c == _t('\f') || c == 0x0085 || c == 0x2028 || c == 0x2029) {
			if (!append_line_break()) break;
		}
		else if (c == _t('\t')) {
			if (!append_character(_t(' '))) break;
		}
		else if (c < 0x20 || (c >= 0x7f && c <= 0x9f)) {
			continue;
		}
		else if (is_high_surrogate(c)) {
			if (i < dirty_text.size() && is_low_surrogate(dirty_text[i])) {
				if (max_char_count - text.size() < 2) break;
				text.push_back(c);
				text.push_back(dirty_text[i++]);
			}
			else if (!append_character(0xfffd)) break;
		}
		else if (is_low_surrogate(c)) {
			if (!append_character(0xfffd)) break;
		}
		else if (!append_character(c)) break;
	}

	return text;
}

bool paste_dirty_text(State& state, str_view dirty_text) { //returns true if it could paste something
	bool res = false;
	u64 text_length_after_removal = safe_subtract0(state.char_text.length(), state.selection.sel_width());
	u64 available_char_count = text_length_after_removal <= state.char_max_sz ? state.char_max_sz - text_length_after_removal : 0;
	size_t max_char_count = (size_t)minimum(available_char_count, (u64)SIZE_MAX);
	bool multiline = GetWindowLongPtr(state.wnd, GWL_STYLE) & ES_MULTILINE;
	str text = sanitize_pasted_text(dirty_text, max_char_count, multiline);
	if (!text.empty()) {
		state.history_helper.record_transient_edit(TextEditEntry::Kind::paste);
		res = insert_character(state, to_utf_str(text), state.selection.x_min(), state.selection.x_max());
	}
	return res;
}

void paste(State& state) {
	bool en_change = false; defer{ if (en_change) { notify_parent(state, EN_CHANGE); state.history_helper.break_group(); } };

	if (IsClipboardFormatAvailable(clipboard_format)) {//NOTE: lines end with \r\n, has null terminator
		if (OpenClipboard(state.wnd)) {
			defer{ CloseClipboard(); };
			if (HGLOBAL clipboard = GetClipboardData(clipboard_format)) {
				if (cstr* clipboard_txt = (cstr*)GlobalLock(clipboard)) {
					defer{ GlobalUnlock(clipboard); };
					SIZE_T clipboard_byte_count = GlobalSize(clipboard);
					if (clipboard_byte_count && clipboard_byte_count % sizeof(*clipboard_txt) == 0) {
						size_t clipboard_char_capacity = clipboard_byte_count / sizeof(*clipboard_txt);
						const cstr* clipboard_start = clipboard_txt;
						const cstr* clipboard_end = clipboard_start + clipboard_char_capacity;
						const cstr* null_terminator = std::find(clipboard_start, clipboard_end, (cstr)0);
						if (null_terminator != clipboard_end)
							en_change = paste_dirty_text(state, str_view{ clipboard_start, (size_t)(null_terminator - clipboard_start) });
					}
				}
			}
		}
	}
}

void cut_selection(State& state) {
	bool en_change = false; defer{ if (en_change) { notify_parent(state, EN_CHANGE); state.history_helper.break_group(); } };
	if (!state.selection.has_selection() || !copy_selection(state)) return;
	state.history_helper.record_transient_edit(TextEditEntry::Kind::cut);
	en_change = remove_selection(state);
}

bool undo(State& state) {
	bool en_change = false; defer{ if (en_change) notify_parent(state, EN_CHANGE); };

	if (state.history.can_undo()) {
		auto entry = state.history.undo();
		en_change = insert_character<false>(state, to_utf_str(entry->removed_text), entry->position_x_min, entry->position_x_min + entry->inserted_text.length());
		if (en_change) {
			set_selection(state, entry->selection_before.anchor, entry->selection_before.cursor,
				entry->selection_before_trailing);
			state.history_helper.break_group();
		}
		else
			state.history.redo(); //modification operation failed, restore history
	}
	return en_change;
}

bool redo(State& state) {
	bool en_change = false; defer{ if (en_change) notify_parent(state, EN_CHANGE); };

	if (state.history.can_redo()) {
		auto entry = state.history.redo();
		en_change = insert_character<false>(state, to_utf_str(entry->inserted_text), entry->position_x_min, entry->position_x_min + entry->removed_text.length());
		if (en_change) {
			set_selection(state, entry->selection_after.anchor, entry->selection_after.cursor,
				entry->selection_after_trailing);
			state.history_helper.break_group();
		}
		else
			state.history.undo(); //modification operation failed, restore history
	}
	return en_change;
}

template<bool ClearHistory = true>
bool set_text(State& state, cstr* buf) {
	bool res = false;
	cstr empty_txt = 0;
	if (!buf) buf = &empty_txt; //NOTE: this is the standard default behaviour
	
	if (auto txt = to_utf_str(buf); txt.cnt() <= (size_t)state.char_max_sz) {
		//TODO(fran): settext should check for invalid characters (use paste_dirty_text())
		state.history_helper.record_transient_edit(TextEditEntry::Kind::programmatic);
		res = insert_character(state, txt, 0, -1);

		if (res) {
			state.history_helper.break_group();
			if constexpr (ClearHistory) state.history.clear();

			ask_window_for_repaint(state.wnd);//TODO(fran): probably unnecessary
		}
	}
	return res;
}

void set_composition_pos(State& state)//TODO(fran): this must set the other stuff too
{
	HIMC imc = ImmGetContext(state.wnd);
	if (imc != NULL) {
		defer{ ImmReleaseContext(state.wnd, imc); };
		COMPOSITIONFORM cf;
		//NOTE immgetcompositionwindow fails
#if 0 //IME window no longer draws the unnecessary border with resizing and button, it is placed at cf.ptCurrentPos and extends down after each character
		cf.dwStyle = CFS_POINT;

		if (GetFocus() == state.wnd) {
			//NOTE: coords are relative to your window area/nc area, _not_ screen coords
			cf.ptCurrentPos.x = state.caret.pos.x;
			cf.ptCurrentPos.y = state.caret.pos.y + state.caret.dim.cy;
		}
		else {
			cf.ptCurrentPos.x = 0;
			cf.ptCurrentPos.y = 0;
		}
#else //IME window no longer draws the unnecessary border with resizing and button, it is placed at cf.ptCurrentPos and has a max size of cf.rcArea in the x axis, the y axis can extend a lot longer, basically it does what it wants with y
		if (!state.hide_IME_wnd) {
			cf.dwStyle = CFS_RECT;

			//TODO(fran): should I place the IME in line with the caret or below so the user can see what's already written in that line?
			if (GetFocus() == state.wnd) {
				cf.ptCurrentPos.x = state.caret.pos.x;
				cf.ptCurrentPos.y = state.caret.pos.y + state.caret.dim.cy;
				//TODO(fran): programatically set a good x axis size
				cf.rcArea = { state.caret.pos.x , state.caret.pos.y + state.caret.dim.cy,state.caret.pos.x + 100,state.caret.pos.y + state.caret.dim.cy + 100 };
			}
			else {
				cf.rcArea = { 0,0,0,0 };
				cf.ptCurrentPos.x = 0;
				cf.ptCurrentPos.y = 0;
			}
		}
		else {
			cf.dwStyle = CFS_RECT | CFS_FORCE_POSITION;
			cf.ptCurrentPos.x = state.caret.pos.x;//INFO: it does _not_ respect x that goes beyond the control
			cf.ptCurrentPos.y = state.caret.pos.y + state.caret.dim.cy;//INFO: it does _not_ respect y that goes beyond the control
			//TODO(fran): programatically set a good x axis size
			cf.rcArea = rectWH(cf.ptCurrentPos.x, cf.ptCurrentPos.y, 1, 1);//INFO: it does _not_ respect w & h, if w or h is too small the window isnt shown, otherwise it's shown in one unique size
#if 0 //this doesnt work, candidate window still shows up
			CANDIDATEFORM cnf;
			cnf.dwIndex = 0;
			cnf.dwStyle = CFS_EXCLUDE;
			cnf.ptCurrentPos = { GetSystemMetrics(SM_CXVIRTUALSCREEN) + 1,GetSystemMetrics(SM_CYVIRTUALSCREEN) + 1 };
			cnf.rcArea = rectWH(GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN), GetSystemMetrics(SM_CXVIRTUALSCREEN) + 500, GetSystemMetrics(SM_CYVIRTUALSCREEN) + 500);
			ImmSetCandidateWindow(imc, &cnf); //this one seems to be the list!
#endif
			//ShowWindow(ImmGetDefaultIMEWnd(state.wnd),SW_HIDE); //maybe I can hide the window? noup
			//ImmSetStatusWindowPos //no idea what this is
		}
#endif
		BOOL res = ImmSetCompositionWindow(imc, &cf); Assert(res);
	}
}

//TODO(fran): the fact that true is hidden and false is shown is very unintuitive and prone to error, reverse it
void set_IME_wnd(HWND wnd, bool hidden) {
	State& state = *get_state(wnd);
	if (&state) {
		state.hide_IME_wnd = hidden;
	}
}

void set_composition_font(State& state)//TODO(fran): this must set the other stuff too
{
	HIMC imc = ImmGetContext(state.wnd);
	if (imc != NULL) {
		defer{ ImmReleaseContext(state.wnd, imc); };
		LOGFONT lf;
		int getobjres = GetObject(state.theme.font, sizeof(lf), &lf); Assert(getobjres == sizeof(lf));

		BOOL setres = ImmSetCompositionFont(imc, &lf); Assert(setres);
	}
}

bool is_placeholder_visible(State& state) {
	bool res = (state.char_text.length() == 0) && *state.placeholder && (GetFocus() != state.wnd || state.maintain_placerholder_on_focus);
	return res;
}

void maintain_placerholder_when_focussed(HWND wnd, bool maintain) {//TODO(fran): idk if this should be standardized and put in the wparam of WM_SETDEFAULTTEXT, it seems kind of annoying especially since this is something you probably only want to set once, different from the text which you may want to change more often
	State& state = *get_state(wnd);
	if (&state) {
		state.maintain_placerholder_on_focus = maintain;
		if (is_placeholder_visible(state)) ask_window_for_repaint(state.wnd);
	}
}

bool is_caret_stop(const State& state, size_t character_index) {
	if (character_index > state.char_text.size()) return false;
	auto [line_start, line_index] = char_idx_to_line_char_idx(state, character_index);
	const TextLineLayout* line = get_line_layout(state, line_index);
	if (!line) return character_index == 0;
	size_t local_index = character_index - line_start;
	return local_index < line->caret_stops.size() && line->caret_stops[local_index];
}

size_t move_to_caret_stop(const State& state, size_t character_index, int direction) {
	Assert(direction == 1 || direction == -1);
	character_index = minimum(character_index, state.char_text.size());
	while (true) {
		if (direction > 0) {
			if (character_index >= state.char_text.size()) return state.char_text.size();
			character_index++;
		}
		else {
			if (!character_index) return 0;
			character_index--;
		}
		if (is_caret_stop(state, character_index)) return character_index;
	}
}

size_t snap_to_caret_stop(const State& state, size_t character_index, int direction) {
	character_index = minimum(character_index, state.char_text.size());
	if (is_caret_stop(state, character_index)) return character_index;
	return move_to_caret_stop(state, character_index, direction);
}

size_t closest_caret_stop(const State& state, size_t character_index) {
	character_index = minimum(character_index, state.char_text.size());
	if (is_caret_stop(state, character_index)) return character_index;
	size_t previous = character_index;
	while (previous && !is_caret_stop(state, previous)) previous--;
	size_t next = character_index;
	while (next < state.char_text.size() && !is_caret_stop(state, next)) next++;
	if (!is_caret_stop(state, previous)) return next;
	if (!is_caret_stop(state, next)) return previous;
	return character_index - previous <= next - character_index ? previous : next;
}

struct TextCaretHit { size_t character_index; bool trailing; };

TextCaretHit closest_caret_stop(const TextLineLayout& line, f32 x) {
	TextCaretHit closest{};
	f32 closest_distance = F32INFINITY;
	for (const auto& stop : line.visual_caret_stops) {
		f32 distance = abs(stop.x - x);
		if (distance < closest_distance) {
			closest = { stop.character_index, stop.trailing };
			closest_distance = distance;
		}
	}
	return closest;
}

size_t visual_caret_stop_index(const TextLineLayout& line, size_t local_index, bool trailing) {
	for (size_t i = 0; i < line.visual_caret_stops.size(); i++) {
		const auto& stop = line.visual_caret_stops[i];
		if (stop.character_index == local_index && stop.trailing == trailing) return i;
	}
	f32 current_x = line_caret_x(line, local_index, trailing);
	size_t closest_index = 0;
	f32 closest_distance = F32INFINITY;
	for (size_t i = 0; i < line.visual_caret_stops.size(); i++) {
		const auto& stop = line.visual_caret_stops[i];
		if (stop.character_index != local_index) continue;
		f32 distance = abs(stop.x - current_x);
		if (distance < closest_distance) {
			closest_index = i;
			closest_distance = distance;
		}
	}
	return closest_index;
}

TextCaretHit move_to_visual_caret_stop(const State& state, TextCaretHit current, int direction) {
	auto [line_start, line_index] = char_idx_to_line_char_idx(state, current.character_index);
	const TextLineLayout* line = get_line_layout(state, line_index);
	if (!line || line->visual_caret_stops.empty()) return current;
	size_t stop_index = visual_caret_stop_index(*line, current.character_index - line_start, current.trailing);
	if (direction > 0 && stop_index + 1 < line->visual_caret_stops.size()) stop_index++;
	else if (direction < 0 && stop_index) stop_index--;
	else {
		int line_direction = line->right_to_left ? -direction : direction;
		if ((line_direction < 0 && !line_index) || (line_direction > 0 && line_index + 1 >= state.line_layouts.size())) return current;
		line_index = line_direction > 0 ? line_index + 1 : line_index - 1;
		line = get_line_layout(state, line_index);
		if (!line || line->visual_caret_stops.empty()) return current;
		size_t target_character = line_direction > 0 ? 0 : line->text_length;
		stop_index = visual_caret_stop_index(*line, target_character, line_direction < 0);
	}
	const auto& stop = line->visual_caret_stops[stop_index];
	return { line->text_start + stop.character_index, stop.trailing };
}

TextCaretHit visual_selection_edge(const State& state, int direction) {
	size_t x_min = state.selection.x_min(), x_max = state.selection.x_max();
	auto min_line = char_idx_to_line_char_idx(state, x_min);
	auto max_line = char_idx_to_line_char_idx(state, x_max);
	if (min_line.line_idx != max_line.line_idx) return { direction > 0 ? x_max : x_min, direction > 0 };
	const TextLineLayout* line = get_line_layout(state, min_line.line_idx);
	if (!line) return { direction > 0 ? x_max : x_min, direction > 0 };
	TextCaretHit result{ direction > 0 ? x_max : x_min, direction > 0 };
	f32 edge_x = direction > 0 ? -F32INFINITY : F32INFINITY;
	for (const auto& stop : line->visual_caret_stops) {
		size_t global_index = line->text_start + stop.character_index;
		if (global_index < x_min || global_index > x_max) continue;
		if ((direction > 0 && stop.x > edge_x) || (direction < 0 && stop.x < edge_x)) {
			edge_x = stop.x;
			result = { global_index, stop.trailing };
		}
	}
	return result;
}

void move_selection(State& state, int direction, bool shift_is_down, bool ctrl_is_down) {
	Assert(direction == 1 || direction == -1);
	size_t anchor;
	TextCaretHit cursor{ state.selection.cursor, state.caret.trailing };
	if (ctrl_is_down) {
		auto line_location = char_idx_to_line_char_idx(state, state.selection.cursor);
		const TextLineLayout* line = get_line_layout(state, line_location.line_idx);
		int logical_direction = line && line->right_to_left ? -direction : direction;
		cursor.character_index = snap_to_caret_stop(state,
			find_stopper(to_utf_str(state.char_text), state.selection.cursor, logical_direction), logical_direction);
		cursor.trailing = logical_direction > 0;
	}
	else cursor = move_to_visual_caret_stop(state, cursor, direction);

	if (shift_is_down) anchor = state.selection.anchor;
	else {
		if (state.selection.has_selection()) cursor = visual_selection_edge(state, direction);
		anchor = cursor.character_index;
	}
	set_selection<true>(state, anchor, cursor.character_index, cursor.trailing);
	state.history_helper.break_group();
}

void move_selection_vertical(State& state, int direction, bool shift_is_down, bool ctrl_is_down) {
	Assert(direction == 1 || direction == -1);
	if (ctrl_is_down) return;
	size_t anchor;
	TextCaretHit cursor{ state.selection.cursor, state.caret.trailing };
	if (!shift_is_down && state.selection.has_selection()) {
		cursor = { direction > 0 ? state.selection.x_max() : state.selection.x_min(), direction > 0 };
	}
	else {
		auto [line_start, line_index] = char_idx_to_line_char_idx(state, state.selection.cursor);
		if ((direction < 0 && !line_index) || (direction > 0 && line_index + 1 >= state.line_layouts.size()))
			cursor = { direction > 0 ? state.char_text.size() : 0, direction > 0 };
		else {
			const TextLineLayout* current_line = get_line_layout(state, line_index);
			f32 current_x = current_line ? line_render_x(state, *current_line) + line_caret_x(*current_line,
				state.selection.cursor - line_start, state.caret.trailing) : 0;
			if (state.vertical_selection_stored_width > current_x) current_x = state.vertical_selection_stored_width;
			else state.vertical_selection_stored_width = current_x;
			size_t target_index = direction > 0 ? line_index + 1 : line_index - 1;
			const TextLineLayout* target_line = get_line_layout(state, target_index);
			if (target_line) {
				auto local_hit = closest_caret_stop(*target_line, current_x - line_render_x(state, *target_line));
				cursor = { target_line->text_start + local_hit.character_index, local_hit.trailing };
			}
		}
	}
	anchor = shift_is_down ? state.selection.anchor : cursor.character_index;
	set_selection<true>(state, anchor, cursor.character_index, cursor.trailing);
	state.history_helper.break_group();
}

TextCaretHit point_to_char(State& state, POINT mouse/*client coords*/) {
	size_t line_index = clamp((size_t)0,
		(size_t)maximum(mouse.y - state.padding.y + state.scroll.y, 0) / (size_t)calc_line_height_px(state),
		state.line_breaks.size());
	const TextLineLayout* line = get_line_layout(state, line_index);
	if (!line) return {};
	f32 x = (f32)(mouse.x + state.scroll.x - line_render_x(state, *line));
	TextCaretHit local_hit = closest_caret_stop(*line, x);

	HDC dc = GetDC(state.wnd); defer{ ReleaseDC(state.wnd,dc); };
	HFONT oldfont = SelectFont(dc, state.theme.font); defer{ SelectFont(dc, oldfont); };
	str rendered_text;
	IDWriteTextLayout* text_layout = nil;
	if (create_line_text_layout(state, dc, *line, &text_layout, rendered_text)) {
		defer{ release_com(text_layout); };
		BOOL trailing = FALSE, inside = FALSE;
		DWRITE_HIT_TEST_METRICS hit{};
		if (text_layout->HitTestPoint(x, 0, &trailing, &inside, &hit) == S_OK) {
			size_t boundary = minimum((size_t)hit.textPosition + (trailing ? hit.length : 0), line->text_length);
			if (boundary < line->caret_stops.size() && line->caret_stops[boundary]) local_hit = { boundary, trailing != FALSE };
		}
	}
	return { line->text_start + local_hit.character_index, local_hit.trailing };
}

//NOTE: Windows had the brilliant idea of stopping the caret from blinking after a couple of seconds, and the only real fix is via a registry hack, therefore we take the poor man's approach, a 5 sec timer that reblinks the caret
void keep_caret_blinking(State& state) {
	SetTimer(state.wnd, EDITONELINE_caret_timer_id, EDITONELINE_default_caret_duration, NULL);
}

void stop_caret_blinking(State& state) {
	KillTimer(state.wnd, EDITONELINE_caret_timer_id);
}

/**
  * mouse: in screen coords
  */
void show_rclickmenu(State& state, POINT mouse) {
	HMENU m = CreateMenu(); defer{ DestroyMenu(m); };
	HMENU subm = CreateMenu();
	AppendMenu(m, MF_POPUP | MF_OWNERDRAW, (UINT_PTR)subm, (LPCWSTR)m);

	bool has_selection = state.selection.has_selection();

	append_item_to_menu(subm, menu::undo, LANG_MENU_EDIT_UNDO, bmps.menu_undo, true);

	append_item_to_menu(subm, menu::redo, LANG_MENU_EDIT_REDO, bmps.menu_redo, true);

	append_separator_to_menu(subm);

	append_item_to_menu(subm, menu::cut, LANG_MENU_EDIT_CUT, bmps.cut, !has_selection);

	append_item_to_menu(subm, menu::copy, LANG_MENU_EDIT_COPY, bmps.clipboard, !has_selection);

	append_item_to_menu(subm, menu::paste, LANG_MENU_EDIT_PASTE, bmps.menu_paste, !IsClipboardFormatAvailable(clipboard_format));

	append_item_to_menu(subm, menu::del, LANG_MENU_EDIT_DELETE, bmps.bin, !has_selection);

	append_separator_to_menu(subm);

	append_item_to_menu(subm, menu::find, LANG_MENU_EDIT_FIND, bmps.menu_search, true);

	append_item_to_menu(subm, menu::select_all, LANG_MENU_EDIT_SELECT_ALL, bmps.menu_select_all, 
		!state.char_text.size() || (has_selection && state.selection.sel_width() >= state.char_text.size())
	);

	set_menu_background_color(m, colors.CaptionBk);

	TrackPopupMenuEx(subm, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_NOANIMATION, mouse.x, mouse.y, state.wnd, 0);
}

void init_cpp_objects(State& state) {
	state.char_text = str();//REMEMBER: this c++ objects dont like being calloc-ed, they need their constructor, or, in this case, someone else's, otherwise they are badly initialized
	state.line_breaks = decltype(state.line_breaks)();
	state.line_layouts = decltype(state.line_layouts)();
	state.history = decltype(state.history)();
	state.ime_composition.removed_text = str();
	state.ime_composition.committed_text = str();
	state.ime_composition.provisional_text = str();
}

void release_cpp_objects(State& state) {
	state.char_text.~basic_string();
	state.line_breaks.~vector();
	state.line_layouts.~vector();
	state.history.release();
	state.ime_composition.removed_text.~basic_string();
	state.ime_composition.committed_text.~basic_string();
	state.ime_composition.provisional_text.~basic_string();
}

//Renders the selection box corresponding to only one line
void render_selection(HDC dc, HBRUSH brush, CharSel sel, State& state, int yPos,
	size_t line_index, IDWriteTextLayout* text_layout)
{
	Assert(sel.has_selection());
	const TextLineLayout* line = get_line_layout(state, line_index);
	if (!line) return;
	size_t local_start = clamp((size_t)0, safe_subtract0(sel.x_min(), line->text_start), line->text_length);
	size_t local_end = clamp((size_t)0, safe_subtract0(sel.x_max(), line->text_start), line->text_length);
	COLORREF sel_col = ColorFromBrush(brush);
	auto draw_rectangle = [&](f32 left, f32 top, f32 right, f32 bottom) {
		RECT selection{
			(LONG)floorf((f32)line_render_x(state, *line) - state.scroll.x + left),
			(LONG)floorf((f32)yPos - state.scroll.y + top),
			(LONG)ceilf((f32)line_render_x(state, *line) - state.scroll.x + right),
			(LONG)ceilf((f32)yPos - state.scroll.y + bottom),
		};
		urender::FillRectAlpha(dc, selection, GetRValue(sel_col), GetGValue(sel_col), GetBValue(sel_col), 128);
	};

	bool text_selection_rendered = false;
	UINT32 metric_count = 0;
	if (text_layout && local_end > local_start) {
		text_layout->HitTestTextRange((UINT32)local_start, (UINT32)(local_end - local_start), 0, 0, nil, 0, &metric_count);
		std::vector<DWRITE_HIT_TEST_METRICS> metrics(metric_count);
		if (metric_count && text_layout->HitTestTextRange((UINT32)local_start, (UINT32)(local_end - local_start),
			0, 0, metrics.data(), metric_count, &metric_count) == S_OK)
		{
			for (const auto& metric : metrics)
				draw_rectangle(metric.left, metric.top, metric.left + metric.width, metric.top + metric.height);
			text_selection_rendered = true;
		}
	}

	if (!text_selection_rendered && local_end > local_start) {
		f32 start_x = line_caret_x(*line, local_start, false);
		f32 end_x = line_caret_x(*line, local_end, true);
		draw_rectangle(minimum(start_x, end_x), 0, maximum(start_x, end_x), (f32)state.caret.dim.cy);
	}

	size_t newline_index = line->text_start + line->text_length;
	bool newline_selected = line_index < state.line_breaks.size() && sel.x_min() <= newline_index && sel.x_max() > newline_index;
	if (newline_selected) {
		SIZE space_size{};
		GetTextExtentPoint32(dc, _t(" "), 1, &space_size);
		f32 end_x = line_caret_x(*line, line->text_length, true);
		f32 width = (f32)maximum(space_size.cx, 1L);
		draw_rectangle(line->right_to_left ? end_x - width : end_x, 0,
			line->right_to_left ? end_x : end_x + width, (f32)state.caret.dim.cy);
	}
	//TODO(fran): benchmark whether doing all the calculations and only rendering one polygon is faster, in which case render_selection would have to take the entire (multiline) selection and convert it into one big polygon
}

void render(State& state, HDC dc) {
	auto& brushes = state.theme.brushes;

	RECT rc; GetClientRect(state.wnd, &rc);
	int w = RECTW(rc), h = RECTH(rc);

	LONG_PTR style = GetWindowLongPtr(state.wnd, GWL_STYLE);
	bool show_placeholder = is_placeholder_visible(state);
	bool is_enabled = IsWindowEnabled(state.wnd);

	u32 border_thickness = state.theme.dimensions.border_thickness;

	HBRUSH bk_br, txt_br, border_br, selection_br;
	HFONT font = state.theme.font;

	if (is_enabled) {
		bk_br = brushes.bk.normal;
		txt_br = brushes.foreground.normal;
		border_br = brushes.border.normal;
		selection_br = (GetFocus() == state.wnd) ? brushes.selection.normal : brushes.selection.disabled;
	}
	else {
		bk_br = brushes.bk.disabled;
		txt_br = brushes.foreground.disabled;
		border_br = brushes.border.disabled;
		selection_br = brushes.selection.disabled;
	}
	if (show_placeholder) {
		txt_br = brushes.placeholder.normal;
		font = fonts.General; //TODO(fran): add placeholder specific font in theme
	}

	urender::draw_background(dc, rc, bk_br, border_br, state.theme.dimensions);

	//TODO(fran): clip text rendering inside drawing area of the background and border (must take into account rounded borders as well)
	{
		HFONT oldfont = SelectFont(dc, font); defer{ SelectFont(dc, oldfont); };
		UINT oldalign = GetTextAlign(dc); defer{ SetTextAlign(dc,oldalign); };

		COLORREF oldtxtcol = SetTextColor(dc, ColorFromBrush(txt_br)); defer{ SetTextColor(dc, oldtxtcol); };
		auto oldbkmode = SetBkMode(dc, TRANSPARENT); defer{ SetBkMode(dc, oldbkmode); };

		TEXTMETRIC tm; GetTextMetrics(dc, &tm);
		// Calculate vertical position for the string so that it will be vertically centered
		// We are single line so we want vertical alignment always
		//TODO(fran): allow the user to select the vertical alignment (top, center, bottom)
		//TODO(fran): BUG: vertical centering doesnt automatically occur when going from 2 lines to 1 by deleting the \n character, it remains with top centering until another letter is written by the user
		int yPos = state.padding.y;
		int xPos;

		if (style & ES_CENTER) {
			SetTextAlign(dc, TA_CENTER);
			xPos = (rc.right - rc.left) / 2;
		}
		else if (style & ES_RIGHT) {
			SetTextAlign(dc, TA_RIGHT);
			xPos = rc.right - state.padding.x;
		}
		else /*ES_LEFT*/ {//NOTE: ES_LEFT==0, that was their way of defaulting to left
			SetTextAlign(dc, TA_LEFT);
			xPos = rc.left + state.padding.x;
		}

		if (show_placeholder) {
			TextOut(dc, xPos, yPos, state.placeholder, (int)cstr_len(state.placeholder));
		}
		else {
			struct RenderedLine {
				IDWriteTextLayout* text_layout;
				str owned_text;
				const TextLineLayout* cached_layout;
				int y;
			};
			std::vector<RenderedLine> rendered_lines;
			rendered_lines.reserve(state.line_layouts.size());
			for (size_t line_index = 0; line_index < state.line_layouts.size(); line_index++) {
				const TextLineLayout& line = state.line_layouts[line_index];
				rendered_lines.push_back({ .text_layout = nil, .cached_layout = &line, .y = yPos });
				auto& rendered = rendered_lines.back();
				create_line_text_layout(state, dc, line, &rendered.text_layout, rendered.owned_text);

				const size_t start = line.text_start, end = start + line.text_length + 1;
				CharSel selection{ clamp(start,state.selection.x_min(),end), clamp(start,state.selection.x_max(),end) };
				if (selection.has_selection())
					render_selection(dc, selection_br, selection, state, yPos, line_index, rendered_lines.back().text_layout);
				yPos += tm.tmHeight;
			}
			auto render_line_with_gdi = [&](const RenderedLine& line) {
				str masked_text;
				const utf16* text = state.char_text.c_str() + line.cached_layout->text_start;
				if (style & ES_PASSWORD) {
					if (line.owned_text.size() == line.cached_layout->text_length) text = line.owned_text.c_str();
					else {
						masked_text.assign(line.cached_layout->text_length, password_char);
						text = masked_text.c_str();
					}
				}
				SetTextAlign(dc, TA_LEFT | (line.cached_layout->right_to_left ? TA_RTLREADING : 0));
				TextOut(dc, line_render_x(state, *line.cached_layout) - state.scroll.x, line.y - state.scroll.y,
					text, (int)line.cached_layout->text_length);
			};

			auto& factories = get_directwrite_factories();
			ID2D1DCRenderTarget* target = nil;
			ID2D1SolidColorBrush* text_brush = nil;
			if (factories.draw) {
				D2D1_RENDER_TARGET_PROPERTIES properties{};
				properties.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
				properties.pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE };
				properties.dpiX = properties.dpiY = 96.f;
				properties.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
				properties.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
				factories.draw->CreateDCRenderTarget(&properties, &target);
			}
			bool directwrite_rendered = false;
			if (target && target->BindDC(dc, &rc) == S_OK) {
				COLORREF color = ColorFromBrush(txt_br);
				D2D1_COLOR_F brush_color{ GetRValue(color) / 255.f, GetGValue(color) / 255.f, GetBValue(color) / 255.f, 1.f };
				target->CreateSolidColorBrush(brush_color, &text_brush);
				if (text_brush) {
					target->BeginDraw();
					target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
					for (const auto& line : rendered_lines) {
						if (!line.text_layout) continue;
						D2D1_POINT_2F origin{
							(f32)(line_render_x(state, *line.cached_layout) - state.scroll.x),
							(f32)(line.y - state.scroll.y),
						};
						target->DrawTextLayout(origin, line.text_layout, text_brush, D2D1_DRAW_TEXT_OPTIONS_NONE);
					}
					directwrite_rendered = target->EndDraw() == S_OK;
				}
			}
			for (const auto& line : rendered_lines)
				if (!directwrite_rendered || !line.text_layout) render_line_with_gdi(line);
			for (auto& line : rendered_lines) release_com(line.text_layout);
			release_com(text_brush);
			release_com(target);
		}
	}
}

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	//static int __c; printf("%d:EDITONELINE:%s\n",__c++, msgToString(msg));

	State& state = *get_state(hwnd); _control_validate_state;
	switch (msg) {
	case WM_NCCREATE:
	{
		CREATESTRUCT* creation_nfo = (CREATESTRUCT*)lparam;

		Assert(!(creation_nfo->style & ES_RIGHT));//TODO(fran)

		auto st = (State*)calloc(1, sizeof(State));
		Assert(st);
		set_window_state(hwnd, st);
		st->wnd = hwnd;
		st->parent = creation_nfo->hwndParent;
		st->identifier = (u32)(UINT_PTR)creation_nfo->hMenu;
		st->char_max_sz = -1;//default established by windows: 32767
		*st->placeholder = 0;
		init_cpp_objects(*st);

		return TRUE;
	} break;
	case WM_CREATE:
	{
		//Create our tooltip
		state.controls.tooltip = CreateWindowEx(WS_EX_TOPMOST/*make sure it can be seen in any wnd config*/, TOOLTIPS_CLASS, NULL,
			WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,//INFO(fran): TTS_BALLOON doesnt work on windows10, you need a registry change https://superuser.com/questions/1349414/app-wont-show-balloon-tip-notifications-in-windows-10 simply amazing
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			state.wnd, NULL, NULL, NULL);
		Assert(state.controls.tooltip);

		TOOLINFO toolInfo{ TTTOOLINFO_V1_SIZE };
		toolInfo.hwnd = state.wnd;
		toolInfo.uFlags = TTF_IDISHWND | TTF_ABSOLUTE | TTF_TRACK /*allows to show the tooltip when we please*/; //TODO(fran): TTF_TRANSPARENT might be useful, mouse msgs that go to the tooltip are sent to the parent aka us
		toolInfo.uId = (UINT_PTR)state.wnd;
		toolInfo.rect = { 0 };
		toolInfo.hinst = GetModuleHandle(NULL);
		toolInfo.lpszText = 0;
		toolInfo.lParam = 0;
		toolInfo.lpReserved = 0;
		BOOL addtool_res = (BOOL)SendMessage(state.controls.tooltip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
		Assert(addtool_res);

		SendMessage(state.controls.tooltip, TTM_SETTIPTEXTCOLOR, ColorFromBrush(colors.ControlTxt), 0);
		SendMessage(state.controls.tooltip, TTM_SETTIPBKCOLOR, ColorFromBrush(colors.ControlBk), 0);

		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SIZE: {
		update_char_pad(state);
		reposition_caret(state, true);

		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_SHOWWINDOW:
	{
		BOOL show = (BOOL)wparam;
		if (!show) {
			//We're going to be hidden, we must also hide our tooltips
			SendMessage(state.wnd, WM_TIMER, EDITONELINE_tooltip_timer_id, 0); //TODO(fran): #robustness, we should know which timers are active to only disable those, also replace this crude SendMessage with a hide_tooltips()
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case EM_LIMITTEXT: //Set text limit in _characters_ ; does NOT include the terminating null
	{
		//TODO(fran): docs says this shouldnt affect text already inside the control nor text set via WM_SETTEXT, not sure I agree with that
		state.char_max_sz = (u32)wparam;
		return 0;
	} break;
	case WM_SETFONT:
	{
		HFONT font = (HFONT)wparam;

		Theme new_theme {.font = font };
		set_theme(state.wnd, new_theme);

		return 0;
	} break;
	case WM_NCDESTROY:
	{
		stop_caret_blinking(state);
		if (state.caret.bmp) {
			DeleteBitmap(state.caret.bmp);
			state.caret.bmp = nil;
		}
		release_cpp_objects(state);
		set_window_state(state.wnd, nil);
		free(&state);
		return 0;
	}break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps; //ps.rcPaint
		HDC dc = BeginPaint(state.wnd, &ps); defer{ EndPaint(state.wnd, &ps); };
		render(state, dc);
		return 0;
	} break;
	case WM_DESIRED_SIZE:
	{
		SIZE* min = (decltype(min))wparam;
		SIZE* max = (decltype(max))lparam;
		min->cy = maximum(min_height(state), (i32)min->cy);
		max->cy = minimum(maximum(desired_height(state), (i32)min->cy), (i32)max->cy);

		//TODO(fran): this needs to be redone and we should return smth like this: struct{padding, increment, increment_cnt}
		// where increment basically indicates the size of a line, and increment_cnt the number of desired lines.
		// Or obviously even better would be if we, as the edit control, could manually select our size based on limitations imposed by our creator

		return DesiredSize::flexible;
	} break;
	case WM_MEASUREITEM:
	{
		return handle_wm_measureitem(hwnd, msg, wparam, lparam);
	} break;
	case WM_DRAWITEM:
	{
		return handle_wm_drawitem(hwnd, msg, wparam, lparam);
	} break;
	case WM_STYLECHANGED:
	{
		if (wparam == GWL_STYLE) {
			auto cmp = (STYLESTRUCT*)lparam;
			if ((cmp->styleOld & ES_PASSWORD) != (cmp->styleNew & ES_PASSWORD)) {
				// style changed between using/not using password view
				recalculate_text_layout(state);
			}
		}
		else if (wparam == GWL_EXSTYLE) {
			auto cmp = (STYLESTRUCT*)lparam;
			if ((cmp->styleOld & WS_EX_RTLREADING) != (cmp->styleNew & WS_EX_RTLREADING))
				recalculate_text_layout(state);
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	case WM_ENABLE:
	{//Here we get the new enabled state
		BOOL now_enabled = (BOOL)wparam;
		ask_window_for_repaint(state.wnd);
		//TODO(fran): Hide caret on now_enabled == false? not sure we need to, since that should also trigger a WM_KILLFOCUS which removes the caret
		return 0;
	} break;
	case WM_CANCELMODE:
	{
		//Stop mouse capture, and similar input related things
		ReleaseCapture();
		state.on_mouse_tracking = false;
		//Sent, for example, when the window gets disabled
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_NCHITTEST:
	{
		return handle_wm_nchittest(state.wnd, lparam);
	} break;
	case WM_SETCURSOR://When the mouse goes over us this is 2nd msg received
	{
		return handle_wm_setcursor(hwnd, msg, wparam, lparam, (HCURSOR)GetClassLongPtr(hwnd, GCLP_HCURSOR));
	} break;
	case WM_MOUSEMOVE:
	{
		//wparam = test for virtual keys pressed
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };//Client coords, relative to upper-left corner of client area

		if (state.on_mouse_tracking) {
			//user is trying to make a selection
			auto cursor = point_to_char(state, mouse);
			set_selection(state, state.selection.anchor, cursor.character_index, cursor.trailing);
		}

		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_MOUSEACTIVATE://When the user clicks on us this is 1st msg received (any click: left, right, etc)
	{
		HWND parent = (HWND)wparam;
		WORD hittest = LOWORD(lparam);
		WORD mouse_msg = HIWORD(lparam);
		SetFocus(state.wnd);//set keyboard focus to us
		reset_vertical_selection_stored_width(state);
		return MA_ACTIVATE; //Activate our window and post the mouse msg
	} break;
	case WM_LBUTTONDOWN://When the user clicks on us this is 2nd msg received (possibly, maybe wm_setcursor again)
	{
		//wparam = test for virtual keys pressed
		POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };//Client coords, relative to upper-left corner of client area

		SetCapture(state.wnd);//We want to keep capturing the mouse while the user is still pressing some button, even if the mouse leaves our client area
		state.on_mouse_tracking = true;

		auto cursor = point_to_char(state, mouse);
		set_selection<true>(state, cursor.character_index, cursor.character_index, cursor.trailing);

		return 0;
	} break;
	case WM_LBUTTONDBLCLK:
	{
		auto text = to_utf_str(state.char_text);
		//HACK: We assume the mouse hasnt moved much since the first click, therefore we do not need to check the mouse position to find out where the cursor is since the first click already set the cursor position
		auto left = snap_to_caret_stop(state, find_stopper(text, safe_add(state.selection.cursor, 1), -1), -1);
		auto right = snap_to_caret_stop(state, find_stopper(text, safe_subtract0(state.selection.cursor, 1), +1), +1);
		set_selection<true>(state, left, right);
	} break;
	case WM_LBUTTONUP:
	{
		ReleaseCapture();
		state.on_mouse_tracking = false;
	} break;
	case WM_RBUTTONUP:
	{
		POINT mouse{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) }; MapWindowPoints(state.wnd, nil, &mouse, 1);
		//TODO(fran): before showing the menu we have to update the selection, but only if the click happened outside of the current selection area. If the user right clicks inside of the currently selected area then we just show the menu
		show_rclickmenu(state, mouse);
		return 0;
	} break;
	case WM_SETFOCUS://SetFocus -> WM_IME_SETCONTEXT -> WM_SETFOCUS
	{
		//We gained keyboard focus
		SIZE caret_dim = calc_caret_dim(state);
		if (caret_dim.cx != state.caret.dim.cx || caret_dim.cy != state.caret.dim.cy) {
			if (state.caret.bmp) {
				DeleteBitmap(state.caret.bmp);
				state.caret.bmp = 0;
			}
			//TODO(fran): we should probably check the bpp of our control's img
			int pitch = caret_dim.cx * 4/*bytes per pixel*/;//NOTE: windows expects word aligned bmps, 32bits are always word aligned
			int sz = caret_dim.cy * pitch;
			u32* mem = (u32*)malloc(sz); defer{ free(mem); };
			COLORREF color = ColorFromBrush(state.theme.brushes.foreground.disabled);

			//IMPORTANT: changing caret color is gonna be a pain, docs: To display a solid caret (NULL in hBitmap), the system inverts every pixel in the rectangle; to display a gray caret ((HBITMAP)1 in hBitmap), the system inverts every other pixel; to display a bitmap caret, the system inverts only the white bits of the bitmap.
			//Aka we either calculate how to arrive at the color we want from the caret's bit flipping (will invert bits with 1) or we give up, we should do our own caret
			//NOTE: since the caret is really fucked up we'll average the pixel color so we at least get grays
			//TODO(fran): decide what to do, the other CreateCaret options work as bad or worse so we have to go with this, do we create a separate brush so the user can mix and match till they find a color that blends well with the bk?
			u8 gray = ((u16)(u8)(color >> 16) + (u16)(u8)(color >> 8) + (u16)(u8)color) / 3;
			color = RGB(gray, gray, gray);

			int px_count = sz / 4;
			for (int i = 0; i < px_count; i++) mem[i] = color;
			state.caret.bmp = CreateBitmap(caret_dim.cx, caret_dim.cy, 1, 32, (void*)mem);
			state.caret.dim = caret_dim;
		}
		BOOL caret_res = CreateCaret(state.wnd, state.caret.bmp, 0, 0);
		Assert(caret_res);

		reposition_caret(state, true);

		BOOL showcaret_res = ShowCaret(state.wnd);
		Assert(showcaret_res);

		//We ask for repainting so placeholders and other focus dependent elements con be re-renderered o hidden
		ask_window_for_repaint(state.wnd);

		keep_caret_blinking(state);

		return 0;
	} break;
	case WM_KILLFOCUS:
	{
		//We lost keyboard focus
		//NOTE(fran): docs says to never display/activate a window here cause we can lock the thread

		DestroyCaret();

		state.history_helper.break_group();

		//We ask for repainting so placeholders and other focus dependent elements con be re-renderered o hidden
		ask_window_for_repaint(state.wnd);

		PostMessage(GetParent(state.wnd), WM_COMMAND, MAKELONG(state.identifier, EN_KILLFOCUS), (LPARAM)state.wnd);

		stop_caret_blinking(state);

		return 0;
	} break;
	case WM_GETDLGCODE:
	{
		LRESULT dialog_code = DLGC_WANTCHARS | DLGC_WANTARROWS | DLGC_HASSETSEL;
		MSG* key_message = (MSG*)lparam;
		if (key_message && key_message->message == WM_KEYDOWN &&
			(key_message->wParam == VK_RETURN || key_message->wParam == VK_ESCAPE))
		{
			dialog_code |= DLGC_WANTMESSAGE;
		}
		return dialog_code;
	} break;
	case WM_KEYDOWN://When the user presses a non-system key this is the 1st msg we receive
	{
		//Notifications:
		bool en_change = false; defer{ if (en_change) notify_parent(state, EN_CHANGE); };

		//Vertical Selection Stored Width:
		bool reset_v_sel_stored_w = true;

		//TODO(fran): process VK_NEXT VK_PREV
		//NOTE: there are some keys that dont get sent to WM_CHAR, we gotta handle them here, also there are others that get sent to both, TODO(fran): it'd be nice to have all the key things in only one place
		//NOTE: for things like _shortcuts_ you wanna handle them here cause on WM_CHAR things like Ctrl+V get translated to something else
		//		also you want to use the uppercase version of the letter, eg case _t('V'):
		char vk = (char)wparam;
		bool ctrl_is_down = HIBYTE(GetKeyState(VK_CONTROL));
		bool shift_is_down = HIBYTE(GetKeyState(VK_SHIFT));
		//printf("%c : %d\n", vk, (i32)vk);
		switch (vk) {
		case VK_HOME://Home //TODO(fran): @multiline
		{
			size_t anchor, cursor;

			if (shift_is_down && ctrl_is_down) {//Make a selection to the start of the text
				anchor = state.selection.anchor;
				cursor = 0;
			}
			else if (shift_is_down) {//Make a selection to the start of the line
				anchor = state.selection.anchor;
				cursor = char_idx_to_line_char_idx(state, state.selection.cursor).line_char_idx;
				//cursor = 0;
			}
			else if (ctrl_is_down) {//Remove selection and Go to the start of the text
				anchor = cursor = 0;
			}
			else {//Remove selection and Go to the start of the line (since we're singleline go to start of text)
				//anchor = cursor = 0;
				anchor = cursor = char_idx_to_line_char_idx(state, state.selection.cursor).line_char_idx;
			}

			set_selection<true>(state, anchor, cursor);
		} break;
		case VK_END://End //TODO(fran): @multiline
		{
			size_t anchor, cursor;

			auto last_cursor_in_line = [](State& state) {
				auto line_idx = char_idx_to_line_char_idx(state, state.selection.cursor).line_idx;
				return line_idx < state.line_breaks.size() ? state.line_breaks[line_idx] : state.char_text.length();
			};

			if (shift_is_down && ctrl_is_down) {//Make a selection to the end of the text
				anchor = state.selection.anchor;
				cursor = state.char_text.length();
			}
			else if (shift_is_down) {//Make a selection to the end of the line
				anchor = state.selection.anchor;
				//cursor = state.char_text.length();
				cursor = last_cursor_in_line(state);
			}
			else if (ctrl_is_down) {//Remove selection and Go to the end of the text
				anchor = cursor = state.char_text.length();
			}
			else {//Remove selection and Go to the end of the line (since we're singleline go to start of text)
				//anchor = cursor = state.char_text.length();
				anchor = cursor = last_cursor_in_line(state);
			}

			set_selection<true>(state, anchor, cursor);
		} break;
		case VK_LEFT://Left arrow
		{
			move_selection(state, -1, shift_is_down, ctrl_is_down);
		} break;
		case VK_RIGHT://Right arrow
		{
			move_selection(state, +1, shift_is_down, ctrl_is_down);
		} break;
		case VK_DOWN://Down arrow
		{
			move_selection_vertical(state, +1, shift_is_down, ctrl_is_down);
			reset_v_sel_stored_w = false;
		} break;
		case VK_UP://Up arrow
		{
			move_selection_vertical(state, -1, shift_is_down, ctrl_is_down);
			reset_v_sel_stored_w = false;
		} break;
		//TODO(fran): do we want to update scrolling to the left when deleting causes the text to get shorter than the right side of the element?
		case VK_DELETE://What in spanish is the "Supr" key (delete character ahead of you)
		{
			if (!state.char_text.empty()) {
				state.history_helper.record_transient_edit(TextEditEntry::Kind::delete_forward);
				if (state.selection.has_selection()) en_change = remove_selection(state);
				else {
					if (ctrl_is_down && shift_is_down) {//delete everything til end of the line
						en_change = remove_selection(state, state.selection.cursor, state.char_text.length());
					}
					else if (ctrl_is_down) {//delete everything up to the next stopper
						auto end = snap_to_caret_stop(state, find_stopper(to_utf_str(state.char_text), state.selection.cursor, +1), +1);
						en_change = remove_selection(state, state.selection.cursor, end);
					}
					else if (shift_is_down) {//save whole line to clipboard and then delete it
						auto sel_all = make_selection(state, 0, -1);
						if (copy_selection(state, sel_all)) en_change = remove_selection(state, sel_all);
					}
					else if (state.selection.cursor < state.char_text.length()) {
						en_change = remove_selection(state, state.selection.cursor, move_to_caret_stop(state, state.selection.cursor, +1));//delete character in front of the cursor
					}
				}
			}
		} break;
		case VK_BACK://Backspace
		{
			//TODO(fran): @feature: if an accent has been selected the delete should cancel the accent instead of deleting a character
			//		eg: keyboard sequence: a´a -> aá ; a´backspace -> a
			if (!state.char_text.empty()) {
				state.history_helper.record_transient_edit(TextEditEntry::Kind::backspace);
				if (state.selection.has_selection()) en_change = remove_selection(state);
				else {
					if (ctrl_is_down && shift_is_down) en_change = remove_selection(state, 0, state.selection.cursor); //Remove every character from cursor to line start
					else if (ctrl_is_down) {
						auto start = snap_to_caret_stop(state, find_stopper(to_utf_str(state.char_text), state.selection.cursor, -1), -1);
						en_change = remove_selection(state, start, state.selection.cursor);
					}
					else if (state.selection.cursor > 0) en_change = remove_selection(state, move_to_caret_stop(state, state.selection.cursor, -1), state.selection.cursor);
				}
			}
		} break;
		case _t('a'):
		case _t('A'):
		{
			if (ctrl_is_down) select_all(state);
		} break;
		case _t('v'):
		case _t('V'):
		{
			if (ctrl_is_down) paste(state);
		} break;
		case _t('c'):
		case _t('C'):
		{
			if (ctrl_is_down) copy_selection(state);
		} break;
		case _t('x'):
		case _t('X'):
		{
			if (ctrl_is_down) cut_selection(state);
		} break;
		case _t('z'):
		case _t('Z'):
		{
			if (ctrl_is_down && shift_is_down) redo(state);
			elif (ctrl_is_down) undo(state);
		} break;
		case (char)VK_PROCESSKEY: //NOTE: you must cast to (char) for this value to match
		{
			//UINT conv_vk = MapVirtualKey(lparam>>16, MAPVK_VSC_TO_VK_EX);//doesnt work for arrow keys, thanks windows
			u16 scancode = (decltype(scancode))(lparam >> 16);

			//TODO(fran): check that the candidates window has some candidates, if it doesnt then we can simply continue as if nothing happened
			if (scancode == 0x148/*up arrow*/) {
				state.ignore_IME_candidates = true;
				//PostMessage(state.wnd, WM_KEYDOWN, VK_UP, lparam); TODO(fran): uncomment this and find out what it was for
				reset_v_sel_stored_w = false;
			}
			if (scancode == 0x150/*down arrow*/) {
				state.ignore_IME_candidates = true;
				//PostMessage(state.wnd, WM_KEYDOWN, VK_DOWN, lparam); TODO(fran): uncomment this and find out what it was for
				reset_v_sel_stored_w = false;
			}

		} break;
		default:
		{
			reset_v_sel_stored_w = false;
		} break;
		}

		if (reset_v_sel_stored_w) reset_vertical_selection_stored_width(state);
		ask_window_for_repaint(state.wnd);//TODO(fran): dont invalidate everything, NOTE: also on each wm_paint the cursor will stop so we should add here a bool repaint; to avoid calling InvalidateRect when it isnt needed
		return 0;
	}break;
	case WM_CUT:
	{
		cut_selection(state);
	} break;
	case WM_COPY:
	{
		copy_selection(state);
	} break;
	case WM_PASTE:
	{
		paste(state);
	} break;
	case WM_UNDO:
	{
		return undo(state);
	} break;
	case WM_REDO:
	{
		return redo(state);
	} break;
	case WM_CHAR://When the user presses a non-system key this is the 2nd msg we receive
	{//NOTE: a WM_KEYDOWN msg was translated by TranslateMessage() into WM_CHAR
		//Notifications:
		bool en_change = false; defer{ if (en_change) notify_parent(state, EN_CHANGE); };


		TCHAR c = (TCHAR)wparam;
		bool ctrl_is_down = HIBYTE(GetKeyState(VK_CONTROL));
		LONG_PTR  style = GetWindowLongPtr(state.wnd, GWL_STYLE);

		//lparam = flags
		switch (c) { //https://docs.microsoft.com/en-us/windows/win32/menurc/using-carets
		case 127://Ctrl + Backspace
		case VK_BACK://Backspace (gets sent as WM_CHAR even though it can be handled in WM_KEYDOWN)
		{
			//already handled on WM_KEYDOWN
		}break;
		case VK_TAB://Tab
		{
			if (!(style & WS_TABSTOP)) {
				//We dont handle tabs for now
				Assert(0);
				goto insert_control_char;
			}
		}break;
		case 0x0A://Linefeed, aka \n (I assume it is unused in modern versions, haven't found a key that triggers it)
		case VK_RETURN://Received when the user presses the "enter" key //Carriage Return aka \r
		{
			//NOTE: I wonder, it doesnt seem to send us \r\n so is that something that is manually done by the control?
			//We dont handle "enter" for now
			//if(state.identifier) //TODO(fran): should I only send notifs if I have an identifier? what do the defaults do?
			PostMessage(GetParent(state.wnd), WM_COMMAND, MAKELONG(state.identifier, EN_ENTER), (LPARAM)state.wnd);

			if (style & ES_MULTILINE) {
				//Windows: CRLF ; Unix/Mac: LF ; Machintosh (pre-OS X aka old): CR | CR = \r = 0x0D = VK_RETURN ; LF = \n = 0x0A
				//IMPORTANT: For the sake of not adding pointless complexity internally the control will always use LF, text pasted that uses CRLF or CR will be converted to LF
				c = '\n';
				goto insert_control_char;
			}
		}break;
		case VK_ESCAPE://Escape
		{
			//TODO(fran): should we do something?
			PostMessage(GetParent(state.wnd), WM_COMMAND, MAKELONG(state.identifier, EN_ESCAPE), (LPARAM)state.wnd);
		}break;
		default:
		{
			//TODO(fran): filter more values
			if (c <= (TCHAR)0x14) break;//control chars
			if (c <= (TCHAR)0x1f) break;//IME

		insert_control_char:

			//TODO(fran): maybe this should be the first check for any WM_CHAR?

			//if (contains_char(c, state.invalid_chars)) {
			//	//display tooltip //TODO(fran): make this into a reusable function
			//	str tooltip_msg = (RS(10) + L" " + state.invalid_chars);
			//	show_tip(state.wnd, tooltip_msg.c_str(), EDITONELINE_default_tooltip_duration, ETP::left | ETP::top);
			//	return 0;
			//}

			//We have some normal character
			//TODO(fran): what happens with surrogate pairs? I dont even know what they are -> READ
			if (safe_subtract0(state.char_text.length(), state.selection.sel_width()) < state.char_max_sz) {
				state.history_helper.record_transient_edit(TextEditEntry::Kind::typing);
				en_change = insert_character(state, c);
			}

		}break;
		}
		ask_window_for_repaint(state.wnd); //TODO(fran): dont invalidate everything
		return 0;
	} break;
	case WM_TIMER:
	{
		WPARAM timerID = wparam;
		switch (timerID) {
		case EDITONELINE_tooltip_timer_id:
		{
			KillTimer(state.wnd, timerID);
			TOOLINFO toolInfo{ TTTOOLINFO_V1_SIZE };
			toolInfo.hwnd = state.wnd;
			toolInfo.uId = (UINT_PTR)state.wnd;
			SendMessage(state.controls.tooltip, TTM_TRACKACTIVATE, FALSE, (LPARAM)&toolInfo);
			return 0;
		} break;
		case EDITONELINE_caret_timer_id:
		{
			KillTimer(state.wnd, timerID);

			//TODO(fran): timing isnt quite right, there's a slight delay between each caret "re-blinking"
			//			  also we should check the registry to find out the caret timeout, 5 sec is the default on w10 but could be different
			HideCaret(state.wnd);
			ShowCaret(state.wnd);

			SetTimer(state.wnd, timerID, EDITONELINE_default_caret_duration, NULL);
			return 0;
		} break;
		default: return DefWindowProc(hwnd, msg, wparam, lparam);
		}
	} break;
	case WM_GETTEXT: //the specified char count must include null terminator, since windows' defaults to force writing it to you
	{
		auto dst_cap = maximum((i32)wparam, 0); // includes null terminator
		cstr* dst = (cstr*)lparam;

		if (!dst || dst_cap <= 0) return 0;

		auto src_len = (i32)state.char_text.length();
		auto copy_cnt = minimum(src_len, dst_cap - 1);

		if (copy_cnt > 0) std::copy_n(state.char_text.c_str(), copy_cnt, dst);

		dst[copy_cnt] = (cstr)0;

		return copy_cnt;
	} break;
	case WM_GETTEXTLENGTH://does not include null terminator
	{
		return state.char_text.length();
	} break;
	case WM_SETTEXT:
	{
		cstr* buf = (cstr*)lparam;//null terminated
		bool notify = wparam; //Custom extension to WM_SETTEXT

		//Notifications:
		bool en_change = false; defer{ if (en_change && notify) notify_parent(state, EN_CHANGE); };

		BOOL res = en_change = set_text(state, buf);
		
		if (res) set_selection(state, 0, 0); //When setting the whole element's text we want to keep the cursor at the beginning //TODO(fran): we could provide a flag to switch between setting the selection at the start or at the end

		return res;
	}break;
	case WM_SYSKEYDOWN://1st msg received after the user presses F10 or Alt+some key
	{
		//TODO(fran): notify the parent?
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_IME_SETCONTEXT://Sent after SetFocus to us (sent the first time on SetFocus)
	{//TODO(fran): lots to discover here
		BOOL is_active = (BOOL)wparam;
		//lparam = display options for IME window
		u32 disp_flags = (u32)lparam;
		//NOTE: if we draw the composition window we have to modify some of the lparam values before calling defwindowproc or ImmIsUIMessage

		//NOTE: here you can hide the default IME window and all its candidate windows

		//TODO(fran): with this I get to hide the candidate window & in set_composition_pos I can "hide" the composition window, only problem left is the up and down arrow keys are still bound to the IME window and interact with the now hidden candidate window, I need to block the up&down arrows from getting to the IME and use them myself, or Create my own IME window that's more configurable
		//TODO(fran): maybe I can intercept the "new candidate request" through WM_IME_NOTIFY or somewhere else, stop it from getting to the IME window and transforming it into a WM_LBUTTONDOWN with up or down arrow as key
#if 1
		if (state.hide_IME_wnd) {
			lparam = 0;//TODO(fran): after this is executed, it takes two WM_IME_SETCONTEXT from two other different controls that _do_ want IME visible for the IME window to fix itself
		}
#endif
		printf("WM_IME_SETCONTEXT: lparam = 0x%x\n", (u32)lparam);
		//If we have created an IME window, call ImmIsUIMessage. Otherwise pass this message to DefWindowProc
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}break;
	case WM_IME_NOTIFY:
	case WM_IME_REQUEST://After Alt+Shift to change the keyboard (and some WM_IMENOTIFY) we receive this msg
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_INPUTLANGCHANGE://After Alt+Shift to change the keyboard (and some WM_IMENOTIFY) and WM_IME_REQUEST we receive this msg
	{
		//wparam = charset of the new locale
		//lparam = input locale identifier... wtf
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_IME_STARTCOMPOSITION://On japanese keyboard, after we press a key to start writing this is 1st msg received
	{
		//doc:Sent immediately before the IME generates the composition string as a result of a keystroke
		//This is a notification to an IME window to open its composition window. An application should process this message if it displays composition characters itself.
		//If an application has created an IME window, it should pass this message to that window.The DefWindowProc function processes the message by passing it to the default IME window.
		set_composition_pos(state);
		set_composition_font(state);//TODO(fran): should I place this somewhere else?
		if (state.ime_composition.active) {
			bool en_change = end_IME_composition(state);
			if (en_change) notify_parent(state, EN_CHANGE);
		}
		begin_IME_composition(state);

		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_IME_COMPOSITION://On japanese keyboard, after we press a key to start writing this is 2nd msg received
	{//doc: sent when IME changes composition status cause of keystroke
		//wparam = DBCS char for latest change to composition string, TODO(fran): find out about DBCS

#if 0 //At this point there's no way of knowing if the change was caused by the user writing or changing candidates
		const char* change; static int cnt;
		printf("WM_IME_COMPOSITION %d:\n", cnt++);
		if (lparam & GCS_COMPATTR) { change = "GCS_COMPATTR"; printf("%s\n", change); }
		if (lparam & GCS_COMPCLAUSE) { change = "GCS_COMPCLAUSE"; printf("%s\n", change); }
		if (lparam & GCS_COMPREADSTR) { change = "GCS_COMPREADSTR"; printf("%s\n", change); }
		if (lparam & GCS_COMPREADATTR) { change = "GCS_COMPREADATTR"; printf("%s\n", change); }
		if (lparam & GCS_COMPREADCLAUSE) { change = "GCS_COMPREADCLAUSE"; printf("%s\n", change); }
		if (lparam & GCS_COMPSTR) { change = "GCS_COMPSTR"; printf("%s\n", change); }
		if (lparam & GCS_CURSORPOS) { change = "GCS_CURSORPOS"; printf("%s\n", change); }
		if (lparam & GCS_DELTASTART) { change = "GCS_DELTASTART"; printf("%s\n", change); }
		if (lparam & GCS_RESULTCLAUSE) { change = "GCS_RESULTCLAUSE"; printf("%s\n", change); }
		if (lparam & GCS_RESULTREADCLAUSE) { change = "GCS_RESULTREADCLAUSE"; printf("%s\n", change); }
		if (lparam & GCS_RESULTREADSTR) { change = "GCS_RESULTREADSTR"; printf("%s\n", change); }
		if (lparam & GCS_RESULTSTR) { change = "GCS_RESULTSTR"; printf("%s\n", change); }
		if (!lparam) { change = "CANCEL IME"; printf("%s\n", change); }
#endif

		if (state.hide_IME_wnd && state.ignore_IME_candidates) {
			state.ignore_IME_candidates = false;
			//TODO(fran): join this with the other cases, for example we probably want to have default handling on lparam==0
			HIMC imc = ImmGetContext(state.wnd);
			if (imc != NULL) {
				defer{ ImmReleaseContext(state.wnd, imc); };
				//we wanna restore the IME's comp string to what it was before the candidate change, we do it by simulating an ESC key press which tells the IME to cancel the candidate selection

				//auto simres = ImmSimulateHotKey(state.wnd, IME_JHOTKEY_CLOSE_OPEN); Assert(simres);//TODO(fran): there's only this one hotkey for jp, really?

				INPUT ip;

				// Set up a generic keyboard event.
				ip.type = INPUT_KEYBOARD;
				ip.ki.wScan = 0; // hardware scan code for key
				ip.ki.time = 0;
				ip.ki.dwExtraInfo = 0;

				// Press the key
				ip.ki.wVk = VK_ESCAPE; // virtual-key code for the key
				ip.ki.dwFlags = 0; // 0 for key press
				SendInput(1, &ip, sizeof(ip));

				// Release the key
				ip.ki.dwFlags = KEYEVENTF_KEYUP; // KEYEVENTF_KEYUP for key release
				SendInput(1, &ip, sizeof(ip));
			}
			//TODO(fran): if the IME input has been accepted (probably GCS_RESULTSTR) then we gotta manually change the selection so only the cursor remains
			return 0;
		}


		if (!state.ime_composition.active)
			begin_IME_composition(state);

		if (!lparam) {
			bool en_change = end_IME_composition(state);
			if (en_change) notify_parent(state, EN_CHANGE);
		}
		else {
			HIMC imc = ImmGetContext(state.wnd);
			if (imc != NULL) {
				defer{ ImmReleaseContext(state.wnd, imc); };

				if (lparam & GCS_RESULTSTR) {
					str result;
					if (get_IME_string(imc, GCS_RESULTSTR, result))
						commit_IME_result(state, result);
				}

				if (lparam & GCS_COMPSTR) {
					str composition;
					if (get_IME_string(imc, GCS_COMPSTR, composition))
						replace_IME_provisional_text(state, composition);
				}
			}
		}

		if (state.hide_IME_wnd && lparam & GCS_RESULTSTR)
			return 0;

		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_IME_CHAR://WM_CHAR from the IME window, this are generated once the user has pressed enter on the IME window, so more than one char will probably be coming
	{
		//UNICODE: wparam=utf16 char ; ANSI: DBCS char
		//lparam= the same flags as WM_CHAR
#ifndef UNICODE
		Assert(0);//TODO(fran): DBCS
#endif 
		if (!state.ime_composition.active && !state.hide_IME_wnd) {
			PostMessage(state.wnd, WM_CHAR, wparam, lparam);
		}

		return 0;//docs dont mention return value so I guess it dont matter
	} break;
	case WM_IME_ENDCOMPOSITION://After the chars are sent from the IME window it hides/destroys itself (idk)
	{
		bool en_change = end_IME_composition(state);
		if (en_change) notify_parent(state, EN_CHANGE);
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	//case WM_IME_CONTROL: //NOTE: I feel like this should be received by the wndproc of the IME, I dont think I can get DefWndProc to send it there for me
	//{
	//	//Used to manually send some msg to the IME window, in my case I use it so DefWindowProc sends my msg to it IME default wnd
	//	return DefWindowProc(hwnd, msg, wparam, lparam);
	//}break;
	case WM_SETDEFAULTTEXT:
	{
		cstr* text = (cstr*)lparam;

		string_copy(state.placeholder, text ? str_view{ text } : str_view{});
		if (is_placeholder_visible(state)) ask_window_for_repaint(state.wnd);
		return 1;
	} break;
	case WM_NOTIFYFORMAT://1st msg sent by our tooltip
	{
		switch (lparam) {
		case NF_QUERY: return sizeof(cstr) > 1 ? NFR_UNICODE : NFR_ANSI; //a child of ours has asked us
		case NF_REQUERY: return SendMessage((HWND)wparam/*parent*/, WM_NOTIFYFORMAT, (WPARAM)state.wnd, NF_QUERY); //NOTE: the return value is the new notify format, right now we dont do notifications so we dont care, but this could be a TODO(fran)
		}
		return 0;
	} break;
	case WM_QUERYUISTATE://Strange msg, it wants to know the UI state for a window, I assume that means the tooltip is asking us how IT should look?
	{
		return UISF_ACTIVE | UISF_HIDEACCEL | UISF_HIDEFOCUS; //render as active wnd, dont show keyboard accelerators nor focus indicators
	} break;
	case WM_NOTIFY:
	{
		NMHDR* msg_info = (NMHDR*)lparam;
		if (msg_info->hwndFrom == state.controls.tooltip) {
			switch (msg_info->code) {
			case NM_CUSTOMDRAW:
			{
				NMTTCUSTOMDRAW* cd = (NMTTCUSTOMDRAW*)lparam;
				//INFO: cd->uDrawFlags = flags for DrawText
				switch (cd->nmcd.dwDrawStage) {
					//TODO(fran): probably case CDDS_PREERASE: for SetBkColor for the background
				case CDDS_PREPAINT:
				{
					return CDRF_NOTIFYITEMDRAW;//TODO(fran): I think im lacking something here, we never get CDDS_ITEMPREPAINT, it's possible the msgs are not sent cause it uses a visual style, in which case it doesnt care about you, we would have to remove it with setwindowtheme, and since there wont be any style we'll have to draw it completely ourselves I guess
				} break;
				case CDDS_ITEMPREPAINT://Setup before painting an item
				{
					SelectFont(cd->nmcd.hdc, state.theme.font);
					SetTextColor(cd->nmcd.hdc, ColorFromBrush(state.theme.brushes.foreground.normal));
					SetBkColor(cd->nmcd.hdc, ColorFromBrush(state.theme.brushes.bk.normal));
					return CDRF_NEWFONT;
				} break;
				default: return CDRF_DODEFAULT;
				}
			} break;
			case TTN_GETDISPINFO:
				Assert(0);
			case TTN_LINKCLICK:
				Assert(0);
			case TTN_POP://Tooltip is about to be hidden
				return 0;
			case TTN_SHOW://Tooltip is about to be shown
				return 0;//here we can change the tooltip's position
			}
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_GETFONT:
	{
		return (LRESULT)state.theme.font;
	} break;
	case WM_DEADCHAR://Very interesting, this is the way that TranslateMessage notifies about things like a tilde being pressed, since this keys arent expected to be directly made into characters on the screen
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	} break;
	case WM_PARENTNOTIFY: //Msgs from our potential childs
	{
		return 0;//what the childs do is completely opaque to us
	} break;
	case WM_COMMAND:
	{
		LRESULT res = 0;
		HWND child = (HWND)lparam;
		if (child) {//Notifs from our childs
			//NOTE: for now childs are completely opaque, we simply work as "pasamanos", TODO(fran): we shouldnt have to do this, what we need is some sort of observer, aka something happens and the control can send the msg directly to a place where it'll be answered, having this parent child relationships is not enough, for example in cases like this were there are lots of things that can be appended to edit controls, searchbar, button, scrollbar, ...
			res = SendMessage(state.parent, WM_COMMAND, wparam, lparam);
		}
		else {//otherwise it's a notif from an accelerator or menu
			bool en_change = false;
			switch (LOWORD(wparam))
			{
			case menu::undo: undo(state); break;
			case menu::redo: redo(state); break;
			case menu::cut:cut_selection(state); break;
			case menu::copy: copy_selection(state); break;
			case menu::paste: paste(state); break;
			case menu::del: 
				if (en_change = state.selection.has_selection()) {
					state.history_helper.record_transient_edit(TextEditEntry::Kind::backspace);
					remove_selection(state);
				} 
				break;
			case menu::find: break; //TODO(fran)
			case menu::select_all: select_all(state); break;
			default: res = SendMessage(state.parent, msg, wparam, lparam);
			}
			if (en_change) notify_parent(state, EN_CHANGE); //There was a change in the text
		}
		return res;
	} break;
	case EM_SETSEL:
	{
		size_t anchor = wparam, cursor = lparam;
		set_selection(state, anchor, cursor); //TODO(fran): probably should have set_selection<true>(...)
		return 0;
	} break;
	case EM_GETSEL://TODO(fran): EM_GETSEL_EX (to be able to request for size_t (possibly 64bit) values)
	{
		DWORD* start = (decltype(start))wparam;
		DWORD* end = (decltype(end))lparam;
		auto selection_start = state.selection.x_min();
		auto selection_end = state.selection.x_max();

		if (start) *start = (DWORD)selection_start;
		if (end) *end = (DWORD)selection_end;

		if (selection_start > U16MAX || selection_end > U16MAX) return -1;
		return MAKELONG((WORD)selection_start, (WORD)selection_end);
	} break;
	case WM_CAPTURECHANGED://We're losing mouse capture
	{
		state.on_mouse_tracking = false;
		return 0;
	} break;
	case WM_DESTROYCLIPBOARD: return 0;
	case WM_RENDERALLFORMATS:
	{
		//When our application is about to be closed windows requests that we "render" all the clipboard formats that we have previously set
		//Problem is this is called when you use Delayed Rendering, by calling SetClipboardData with a null hMem parameter, we never use that. So I can only assume two things: either this is a windows bug or TODO(fran): we got a bug
		//Extra info on 'rendering': https://docs.microsoft.com/en-us/windows/win32/dataxchg/using-the-clipboard?redirectedfrom=MSDN#_win32_Copying_Information_to_the_Clipboard
		return 0;
	} break;
	case WM_MOUSEWHEEL:
	{
		// No reason for processing mousewheel input for single line editors
		// TODO(fran): we do want to process it for multi-line editors, vertical scrolling definitely and possibly horizontal too depending on whether word wrapping is enabled
		return DefWindowProc(hwnd, msg, wparam, lparam);//propagates the msg to the parent
	} break;

	default:
	{

#ifdef _DEBUG_HWND_MESSAGES
		if (msg >= 0xC000 && msg <= 0xFFFF) {//String messages for use by applications  
			TCHAR arr[256];
			int res = GetClipboardFormatName(msg, arr, 256);
			cstr_printf(arr); printf("\n");
			//After Alt+Shift to change the keyboard (and some WM_IMENOTIFY) we receive "MSIMEQueryPosition"
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}

		//Assert(0);
#else 
		return DefWindowProc(hwnd, msg, wparam, lparam);
#endif
	} break;
	}
	return 0;
}

struct pre_post_main {
	pre_post_main() { init_wndclass(wndclass, proc, CS_DBLCLKS, IDC_IBEAM); }
	~pre_post_main() {}
} static const PREMAIN_POSTMAIN;
}
