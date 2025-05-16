#include "Load_UMX.h"

#include <fstream>
#include <string>
#include <vector>

using namespace std;

#include "Load_MOD.h"
#include "Load_MTM.h"
#include "Load_S3M.h"
#include "Load_IT.h"

#include "module.h"

namespace MultiPLAY
{
	namespace
	{
		// This code doesn't even try to support MSB platforms.

		int read_int32(std::ifstream &file)
		{
			int value;

			file.read((char *)&value, 4);

			return value;
		}

		int read_compressed_unreal_integer(std::ifstream &file) // "index"
		{
			char b = file.get();

			bool is_signed = (b & 0x80) != 0;
			
			int result = b & 0x3F;
			int shift = 6;

			if (b & 0x40)
			{
				do
				{
					b = file.get();

					result |= int(b & 0x7F) << shift;

					shift += 7;
					/* code */
				} while (((b & 0x80) != 0) && (shift < 32));
			}

			if (is_signed)
			{
				if (result + 1 < result)
					return result + 1;
				else
					return -result;
			}
			else
				return result;
		}

		std::string read_name(std::ifstream &file, int package_version)
		{
			std::string buffer;

			if (package_version >= 64)
			{
				// this is nominally the string length, but the string is guaranteed to be 0-terminated anyway
				buffer.reserve(unsigned(read_compressed_unreal_integer(file)));
			}

			while (true)
			{
				char ch = file.get();

				if (ch == '\0')
					break;

				buffer.append(1, ch);
			}

			return buffer;
		}

		bool equals_case_insensitive_ch(char a, char b)
		{
			return std::tolower(a) == std::tolower(b);
		}

		bool equals_case_insensitive(const std::string &a, const std::string &b)
		{
			return (a.size() == b.size()) && std::equal(a.begin(), a.end(), b.begin(), equals_case_insensitive_ch);
		}

		bool umx_dig_to_export(std::ifstream &file)
		{
			char header_bytes[36];

			file.read(header_bytes, 36);

			if (file.gcount() != 36)
				throw "Partial read";

			unsigned int signature = *(unsigned int *)&header_bytes[0];

			if (signature != 0x9E2A83C1u)
				throw "Signature not present, are you sure this is a UMX file?";

			int package_version = *(int *)&header_bytes[4];

			unsigned names_count = *(unsigned *)&header_bytes[12];
			unsigned names_offset = *(unsigned *)&header_bytes[16];

			std::vector<std::string> names;

			names.reserve(names_count);

			file.seekg(names_offset);

			for (unsigned i=0; i < names_count; i++)
			{
				names.push_back(read_name(file, package_version));

				// skip flags
				read_int32(file);
			}

			int export_count = *(int *)&header_bytes[20];
			int export_offset = *(int *)&header_bytes[24];

			file.seekg(export_offset);

			int export_class = read_compressed_unreal_integer(file);
			int export_super = read_compressed_unreal_integer(file);
			int group = (package_version >= 60) ? read_int32(file) : -1;
			int name_index = read_compressed_unreal_integer(file);
			int flags = read_int32(file);
			int data_size = read_compressed_unreal_integer(file);
			int data_offset = read_compressed_unreal_integer(file);

			if (package_version < 40)
				data_offset += 8;
			if (package_version < 60)
				data_offset += 16;

			file.seekg(data_offset);

			// Skip export item header
			read_compressed_unreal_integer(file);

			if (package_version >= 120) // UT2003
			{
				read_compressed_unreal_integer(file);
				read_int32(file);
				read_int32(file);
			}
			else if (package_version >= 100) // AAO
			{
				read_int32(file);
				read_compressed_unreal_integer(file);
				read_int32(file);
			}
			else if (package_version >= 62) // UT
			{
				read_compressed_unreal_integer(file);
				read_int32(file);
			}
			else // original format
				read_compressed_unreal_integer(file);

			data_size = read_compressed_unreal_integer(file);

			return true;
		}

		bool looks_like_s3m_start(char *signature)
		{
			if (signature[28] != 0x1A)
				return false;

			for (int i=0; i < 27; i++)
			{
				unsigned char ch = (unsigned char)signature[i];

				bool nul = (ch == 0);
				bool printable = ((ch >= 32) && (ch <= 126)) || (ch >= 128);

				if (!nul && !printable)
					return false;
			}

			return true;
		}

		module_struct *load_umx_impl(std::ifstream &file)
		{
			if (!umx_dig_to_export(file))
				throw "Package does not contain a Music directory";

			char signature[30];

			auto start = file.tellg();

			file.read(signature, 30);

			if (file.gcount() != 30)
				throw "Partial read";
			
			file.seekg(start);

			if ((signature[0] == 'M') && (signature[1] == 'T') && (signature[2] == 'M'))
				return load_mtm(&file);

			if ((signature[0] == 'I') && (signature[1] == 'M') && (signature[2] == 'P') && (signature[3] == 'M'))
				return load_it(&file, (int)start);

			if (looks_like_s3m_start(signature))
				return load_s3m(&file);

			try
			{
				return load_mod(&file);
			}
			catch (...)
			{
				throw "Couldn't figure out what kind of file the UMX contains";
			}
		}
	}

	module_struct *load_umx(std::ifstream *file)
	{
		return load_umx_impl(*file);
	}
}