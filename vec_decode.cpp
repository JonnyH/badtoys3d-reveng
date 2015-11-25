#include <iostream>
#include <fstream>
#include <cstdint>
#include <string>
#include <memory>
#include <array>
#include <algorithm>
#include <cstring>
#include <cassert>

#include "lodepng.h"

#include "lodepng.cpp"

struct data_pair
{
	uint8_t row_skip;
	uint8_t copy_count;
};

static_assert(sizeof(data_pair) == 2, "data_pair wrong size");

struct image
{
	std::array<uint16_t,64> offsets;
	std::array<data_pair,64> data_pairs;
	uint8_t data[0];
};
static_assert(sizeof(image) == 256, "image wrong size");

struct rgb
{
	uint8_t r, g, b;
};
static_assert(sizeof(rgb) == 3, "rgb wrong size");

struct rgba
{
	uint8_t r, g, b, a;
	rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) : r(r),g(g),b(b),a(a){}
	rgba() : rgba(0,0,0,0){}
};
static_assert(sizeof(rgba) == 4, "rgba wrong size");

struct palette
{
	rgb c[256];
};
static_assert(sizeof(palette) == 256*3, "palette wrong size");

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		std::cerr << "Must specify input and palette files\n";
		return 1;
	}

	std::ifstream inFile(argv[1]);
	if (!inFile)
	{
		std::cerr << "Failed to open \"" << argv[1] << "\"\n";
		return 1;
	}


	inFile.seekg(0, inFile.end);
	size_t length = inFile.tellg();
	inFile.seekg(0, inFile.beg);

	std::unique_ptr<char[]> data(new char[length]);

	if (!inFile.read(data.get(), length))
	{
		std::cerr << "Failed to read " << length << " bytes from \"" << argv[1] << "\"\n";
		return 1;
	}
	std::cerr << "Read " << length << " bytes from \"" << argv[1] << "\"\n";

	palette p;
	std::ifstream inPalette(argv[2]);
	if (!inPalette)
	{
		std::cerr << "Failed to open palette \"" << argv[2] << "\"\n";
		return 1;
	}

	if (!inPalette.read(reinterpret_cast<char*>(&p), sizeof(p)))
	{
		std::cerr << "Failed to read palette from \"" << argv[2] << "\"\n";
		return 1;
	}

	struct image *img = reinterpret_cast<struct image*>(data.get());

	int sizeX = 64;
	int sizeY = 64;

	std::vector<uint8_t> image_data(sizeX*sizeY*sizeof(rgba));

	for (auto &v : image_data)
		v = 0;

	rgba* rgba_image_data = reinterpret_cast<rgba*>(image_data.data());

	//Each record describes a single column
	for (int record = 0; record < 64; record++)
	{
		//There are 'copy_count' pixels in each column, starting after skipping 'row_skip' transparent pixels
		for (int b = 0; b < img->data_pairs[record].copy_count; b++)
		{
			int offset = img->offsets[record] + b;
			//The offset is in the file, but the data[] array starts at the end of the (256-byte) header, so take the header size off the offset
			offset -= 256;
			assert (offset < length);
			uint8_t idx = img->data[offset];
			rgba pix;
			//index 255 is transparent
			if (idx == 255)
				pix = {0,0,0,0};
			else
				pix = {p.c[idx].r, p.c[idx].g, p.c[idx].b, 255};

			int y = img->data_pairs[record].row_skip + b;
			int x = record;
			rgba_image_data[y*sizeX + x] = pix;
		}
		//Any pixels after a column's 'copy_count' are transparent too
	}

	lodepng::encode("out.png", image_data, sizeX, sizeY);

	return 0;
}
