#pragma once
#include "platform.h"
#include "math.h"

/*
Example usage:

struct text_edit {
	enum class type { insert, erase } type;
	size_t position;
	str text;
};

undo_history<text_edit> history;
history.set_limits(Megabytes(8), 1000); // 8 MiB or 1000 entries, whichever is reached first

text_edit edit{ text_edit::type::insert, position, inserted_text };
size_t edit_memory_size = sizeof(edit) + edit.text.capacity() * sizeof(str::value_type);
history.push(std::move(edit), edit_memory_size);

if (auto* edit = history.undo()) apply_inverse(*edit);
if (auto* edit = history.redo()) apply(*edit);

if (auto* latest = history.latest(); latest && can_merge(*latest, new_char)) {
	history.update_latest([&](text_edit& edit) {
		edit.text.push_back(new_char);
		return sizeof(edit) + edit.text.capacity() * sizeof(str::value_type);
	});
}

Calling push or update_latest after one or more undos discards the redo branch. The memory size
associated with each entry is an estimate supplied by the caller. A limit of 0 means unlimited.
Pointers returned by undo and redo remain valid until an operation modifies the history's vector.
If an updater throws, it must leave the entry unchanged.
*/

template<typename T>
struct undo_history {
	struct history_entry { T value; size_t memory_size; };

	std::vector<history_entry> entries;
	size_t first{};
	size_t next{};
	size_t memory_size{};
	size_t retired_memory_size{};
	size_t max_memory_size{};
	size_t max_entry_count{};

	size_t size() const { return entries.size() - first; }
	size_t undo_size() const { return next - first; }
	size_t redo_size() const { return entries.size() - next; }

	bool empty() const { return first == entries.size(); }
	bool can_undo() const { return next > first; }
	bool can_redo() const { return next < entries.size(); }
	const T* latest() const { return can_undo() ? &entries[next - 1].value : nil; }

	T* undo() {
		return can_undo() ? &entries[--next].value : nil;
	}

	T* redo() {
		return can_redo() ? &entries[next++].value : nil;
	}

	template<typename Func>
	requires requires(Func&& func, T& value) {
		{ std::forward<Func>(func)(value) } -> std::same_as<size_t>;
	}
	bool update_latest(Func&& update) {
		if (!can_undo()) return false;

		auto& entry = entries[next - 1];
		auto old_memory_size = entry.memory_size;
		auto new_memory_size = std::forward<Func>(update)(entry.value);

		discard_redo();

		memory_size -= old_memory_size;
		entry.memory_size = new_memory_size;
		memory_size += entry.memory_size;
		trim_to_limits();
		return can_undo();
	}

	void push(T value, size_t entry_memory_size) {
		discard_redo();
		entries.emplace_back(std::move(value), entry_memory_size); // WARNING: if this line fails the redo history is lost
		memory_size += entry_memory_size;
		next = entries.size();
		trim_to_limits();
	}

	void set_limits(size_t new_max_memory_size, size_t new_max_entry_count) {
		max_memory_size = new_max_memory_size;
		max_entry_count = new_max_entry_count;
		trim_to_limits();
	}

	void clear() {
		entries.clear();
		first = next = memory_size = retired_memory_size = 0;
	}

	// release = clear + free (can be an expensive operation, use only when there's real need to manually free memory).
	void release() {
		clear();
		entries = decltype(entries){}; // Replace the current container with a new one, freeing the allocated capacity of the old one
	}

	void compact() {
		if (!first) return;

		entries.erase(entries.begin(), entries.begin() + first);
		next -= first;
		first = 0;
		retired_memory_size = 0;
	}

private:
	bool over_limit() const {
		return (max_memory_size && memory_size > max_memory_size) || (max_entry_count && size() > max_entry_count);
	}

	void discard_redo() {
		for (auto i = next; i < entries.size(); i++) memory_size -= entries[i].memory_size;

		entries.erase(entries.begin() + next, entries.end());
	}

	void trim_to_limits() {
		while (over_limit()) {
			if (first < next) {
				memory_size -= entries[first].memory_size;
				retired_memory_size += entries[first].memory_size;
				first++;
			}
			elif (next < entries.size()) {
				memory_size -= entries.back().memory_size;
				entries.pop_back();
			}
			else break;
		}

		bool compact_for_entries = first >= 256 && first * 2 >= entries.size();
		bool compact_for_memory = max_memory_size && retired_memory_size >= maximum(max_memory_size / 4, 1);

		if (first == entries.size() || compact_for_entries || compact_for_memory) compact();
	}
};
