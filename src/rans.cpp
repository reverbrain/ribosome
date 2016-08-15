#include "ribosome/rans.hpp"

#include <boost/program_options.hpp>

#include <fstream>
#include <iostream>
#include <sstream>

using namespace ioremap;

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("RANS arithmetic coder options");
	generic.add_options()
		("help", "this help message")
		;

	std::vector<std::string> inames;
	bpo::options_description hidden("Input options");
	hidden.add_options()
		("input-file", bpo::value<std::vector<std::string>>(&inames)->composing(), "input file")
		;

	std::string save_stats_file, load_stats_file;
	bpo::options_description gr("Statistics options");
	gr.add_options()
		("save-stats", bpo::value<std::string>(&save_stats_file), "gather stats from given indexes and save to this file")
		("load-stats", bpo::value<std::string>(&load_stats_file), "load previously saved stats from given file")
		;

	bpo::positional_options_description p;
	p.add("input-file", -1);

	bpo::options_description cmdline_options;
	cmdline_options.add(generic).add(gr).add(hidden);

	bpo::variables_map vm;

	try {
		bpo::store(bpo::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm);

		if (vm.count("help")) {
			std::cout << cmdline_options << std::endl;
			return 0;
		}

		bpo::notify(vm);
	} catch (const std::exception &e) {
		std::cerr << "Invalid options: " << e.what() << "\n" << cmdline_options << std::endl;
		return -EINVAL;
	}

	if (save_stats_file.empty() && load_stats_file.empty()) {
		std::cerr << "You must specify either save or load file for rANS statistics\n" << cmdline_options << std::endl;
		return -EINVAL;
	}

	inames = vm["input-file"].as<std::vector<std::string>>();
	if (inames.empty())
		return 0;

	ribosome::rans rans;
	if (load_stats_file.size()) {
		std::ifstream in(load_stats_file.c_str());
		std::ostringstream ss;
		ss << in.rdbuf();
		std::string ls = ss.str();
		auto err = rans.load_stats(ls.data(), ls.size());
		if (err) {
			std::cerr << "Could not load stats: " << err.message() << ", code: " << err.code() << std::endl;
			return err.code();
		}
	}

	for (auto &iname: inames) {
		std::ifstream in(iname.c_str());
		std::ostringstream ss;
		ss << in.rdbuf();
		std::string data = ss.str();

		if (save_stats_file.size()) {
			rans.gather_stats((const uint8_t *)data.data(), data.size());
		} else {
			size_t offset = 0;
			std::vector<uint8_t> encoded;
			auto err = rans.encode_bytes((const uint8_t *)data.data(), data.size(), &encoded, &offset);
			if (err) {
				std::cerr << "file: " << iname << ": could not encode data: " << err.message() << std::endl;
				return err.code();
			}

			long new_size = encoded.size() - offset;
			float gain = ((long)data.size() - new_size) * 100.0 / data.size();

			std::cout << "file: " << iname <<
				", size: " << data.size() << " -> " << new_size <<
				", gain: " << gain << "%" <<
				std::endl;

			std::vector<uint8_t> decoded;
			decoded.resize(data.size());

			err = rans.decode_bytes(encoded.data() + offset, encoded.size() - offset, &decoded);
			if (err || decoded.size() != data.size() || memcmp(decoded.data(), data.data(), data.size())) {
				std::cerr << "file: " << iname <<
					", data mismatch: " <<
					"orig size: " << data.size() <<
					", decoded size: " << decoded.size() <<
					", err: " << err.message() <<
					std::endl;
				return err.code();
			}
		}
	}

	if (save_stats_file.size()) {
		std::string ds = rans.save_stats();
		if (ds.size() == 0) {
			std::cerr << "Invalid zero stats" << std::endl;
			return -EINVAL;
		}

		std::ofstream out(save_stats_file.c_str(), std::ios::trunc);
		out.write(ds.data(), ds.size());
		out.flush();
	}

	return 0;
}
