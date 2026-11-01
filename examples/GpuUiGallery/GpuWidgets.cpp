#include "GpuWidgets.h"
#include <cmath>

namespace Upp {

Image GpuWidgets::MakePreview()
{
	// Immutable sample pixels, created once and uploaded as an ordinary Image.
	// No native/GDI drawing, image file, external asset or procedural frame work.
	ImageBuffer image(320, 180);
	for(int y = 0; y < 180; ++y) for(int x = 0; x < 320; ++x) {
		Color color = Blend(Color(40, 68, 83), Color(190, 211, 157), y * 255 / 179);
		if((x - 245) * (x - 245) + (y - 45) * (y - 45) < 22 * 22)
			color = Color(239, 217, 162);
		if(y > 122 + 18 * std::sin(x / 48.0)) color = Color(73, 104, 87);
		if(y > 149 + 12 * std::sin(x / 39.0 + 1)) color = Color(35, 65, 61);
		RGBA& pixel = image[y][x];
		pixel.r = color.GetR(); pixel.g = color.GetG(); pixel.b = color.GetB(); pixel.a = 255;
	}
	return image;
}

GpuWidgets::GpuWidgets()
{
	headings_[0].SetText("Tree");
	headings_[1].SetText("Table");
	headings_[2].SetText("Image and controls");
	{
		UiTreeModel& model = tree.Model();
		UiModelUpdate update(model);
		const char *groups[] = { "Scene", "Resources" };
		const char *items[][3] = { { "Particles", "Grid", "Inspector" },
		                          { "Images", "Glyph atlas", "Vectors" } };
		for(int group = 0; group < 2; ++group) {
			UiTreeNodeRef parent = model.AddChild(model.Root(), UiModelItem(groups[group]));
			for(int i = 0; i < 3; ++i) {
				UiModelItem item(items[group][i]);
				item.has_check = true; item.checked = i != 2; item.editable = true;
				model.AddChild(parent, item);
			}
			tree.Expand(parent);
		}
	}
	tree.EnableInternalMutation().EnableRenameOnDblClick();
	{
		UiTableModel& model = table.Model();
		UiModelUpdate update(model);
		model.SetSize(4, 3);
		model.SetHeader(UITABLE_COLUMN_AXIS, 0, UiTableHeader("Item"));
		model.SetHeader(UITABLE_COLUMN_AXIS, 1, UiTableHeader("State"));
		model.SetHeader(UITABLE_COLUMN_AXIS, 2, UiTableHeader("Value"));
		const char *items[] = { "Scene", "Image", "Glyphs", "Dialog" };
		for(int row = 0; row < 4; ++row) {
			model.SetCellValue(row, 0, items[row]);
			model.SetCellValue(row, 1, "Ready");
			model.SetCellValue(row, 2, row + 1);
		}
	}
	table.EnableInternalMutation().SetActiveCell(0, 0);
	table.SetColumnWidth(0, DPI(82)); table.SetColumnWidth(1, DPI(70));
	table.SetColumnWidth(2, DPI(62));
	image_.SetIcon(MakePreview()).SetIconSize(Size(DPI(240), DPI(135)))
	      .SetIconScaleToContent(true);
	caption_.SetText("One image upload / Vulkan drawing");
	image_enabled_.SetText("Enable image"); image_enabled_.SetData(true);
	image_enabled_.WhenAction = [=] { image_.Enable((bool)image_enabled_.GetData()); };
	progress_.Set(67, 100); progress_.SetText("Example progress: 67%");
	for(int i = 0; i < 3; ++i) Add(headings_[i]);
	Add(tree); Add(table); Add(image_); Add(caption_); Add(image_enabled_); Add(progress_);
}

void GpuWidgets::Layout()
{
	const Size s = GetSize();
	const int pad = DPI(10), gap = DPI(12);
	const int column = max(0, (s.cx - 2 * pad - 2 * gap) / 3);
	const int y = pad + DPI(30), height = max(0, s.cy - y - pad);
	for(int i = 0; i < 3; ++i)
		headings_[i].SetRect(pad + i * (column + gap), pad, column, DPI(24));
	tree.SetRect(pad, y, column, height);
	table.SetRect(pad + column + gap, y, column, height);
	const int x = pad + 2 * (column + gap), image_h = min(DPI(135), max(0, height - DPI(116)));
	image_.SetRect(x, y, column, image_h);
	caption_.SetRect(x, y + image_h + DPI(6), column, DPI(36));
	image_enabled_.SetRect(x, y + image_h + DPI(46), column, DPI(28));
	progress_.SetRect(x, y + image_h + DPI(84), column, DPI(26));
}
}
