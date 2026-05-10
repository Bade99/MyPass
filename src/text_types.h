#pragma once

namespace edit_oneline {
	struct Theme {
		union {
			struct {
				brush_group foreground, bk, border, selection, placeholder; //NOTE: for now we use the border color for the caret color
			};
			brush_group all[5];
		private: void _() { static_assert(sizeof(all) == sizeof(*this)); }
		} brushes;
		
		struct {
			u32 border_thickness = U32MAX;
			UINumber border_radius{ .value = F32INFINITY };
		} dimensions;
		HFONT font = nullptr;

		bool copy_from(const Theme& src) {
			bool repaint = false;
			_theme_copy_all_brushes(src.brushes, this->brushes);

			_theme_copy_u32(src.dimensions.border_thickness, this->dimensions.border_thickness);
			_theme_copy_uinumber(src.dimensions.border_radius, this->dimensions.border_radius);

			_theme_copy_pointer(src.font, this->font);

			return repaint;
		}
	};
}