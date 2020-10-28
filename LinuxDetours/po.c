#include <boost/program_options.hpp>

#include <iostream>

int main(int argc, const char* argv[])
{
    using namespace boost::program_options;
    options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message")
        ("person,p", value<std::string>()->default_value("world"), "who")
        ;

    variables_map vm;
    store(parse_command_line(argc, argv, desc), vm);
    notify(vm);

    if (vm.count("help")) {
        std::cout << desc;
        return 0;
    }

    return 0;
}
