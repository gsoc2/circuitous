/*
 * Copyright (c) 2022 Trail of Bits, Inc.
 */
#include <circuitous/IR/Verify.hpp>
#include <circuitous/IR/SMT.hpp>
#include <circuitous/Transforms.hpp>
#include <circuitous/IR/Cost.hpp>

#include <circuitous/Printers/Verilog.hpp>
#include <circuitous/Util/Warnings.hpp>
#include <circuitous/Util/CmdParser.hpp>

#include <circuitous/Support/Ciff.hpp>
#include <circuitous/Support/CLIArgs.hpp>
#include <circuitous/Support/Log.hpp>
#include <circuitous/Support/Check.hpp>

#include <circuitous/Lifter/CircuitSmithy.hpp>

#include <circuitous/Decoder/SemanticsPrinter.hpp>
#include <circuitous/Decoder/DecoderPrinter.hpp>

CIRCUITOUS_RELAX_WARNINGS
#include <gflags/gflags.h>
#include <glog/logging.h>
CIRCUITOUS_UNRELAX_WARNINGS

#include <circuitous/Printers.hpp>
#include <iostream>
#include <remill/OS/OS.h>
#include <unordered_map>

// TODO(lukas): Clean this up once remill gets rid of gflags.
DEFINE_string(arch, "", "");
DEFINE_string(os, REMILL_OS, "");

DEFINE_string(ir_in, "", "Path to a file containing serialized IR.");
DEFINE_string(smt_in, "", "Path to the input smt2 file.");

DEFINE_string(dec_out, "", "Path to the output decoder file.");
DEFINE_string(dot_out, "", "Path to the output GraphViz DOT file.");
DEFINE_string(dot_highlight, "", "Names of node-type to highlight in DOT file");


DEFINE_string(bytes_in, "", "Hex representation of bytes to be lifted");
DEFINE_string(ciff_in, "", "Load input from circuitous-seed --dbg produced file");



namespace cli = circ::cli;

std::string_view as_string_view(std::vector< uint8_t > &buf)
{
    return std::string_view( reinterpret_cast<char *>(buf.data()), buf.size());
}

using input_options = circ::tl::TL<
    cli::CiffIn,
    cli::IRIn,
    cli::SMTIn,
    cli::BytesIn
>;

using output_options = circ::tl::TL<
    circ::cli::DecoderOut,
    circ::cli::DotOut,
    circ::cli::DotHighlight
>;

using remill_config_options = circ::tl::TL<
    circ::cli::Arch,
    circ::cli::OS
>;

using other_options = circ::tl::TL<
    circ::cli::Help,
    circ::cli::Version
>;

using cmd_opts_list = circ::tl::merge<
    input_options,
    output_options,
    remill_config_options,
    other_options
>;

circ::CircuitPtr get_input_circuit(auto &cli)
{
    auto make_circuit = [&](auto buf) {
        circ::log_info() << "Going to make circuit";
        circ::Ctx ctx{ *cli.template get< cli::OS >(), *cli.template get< cli::Arch >() };
        return circ::CircuitSmithy(std::move(ctx)).smelt(buf).forge();
    };

    if (auto bytes = cli.template get< cli::BytesIn >())
        return make_circuit(as_string_view(*bytes));

    if (auto ir_file = cli.template get< cli::IRIn >())
        return circ::Circuit::deserialize(*ir_file);
    if (auto smt_file = cli.template get< cli::SMTIn >())
        return circ::smt::deserialize(*smt_file);

    if (auto cif = cli.template get< cli::CiffIn >())
        return make_circuit(circ::CIFFReader().read(*cif).take_bytes());
    return {};
}


template< typename OptsList >
auto parse_and_validate_cli(int argc, char *argv[]) -> std::optional< circ::ParsedCmd >
{
    using namespace circ;

    static const auto yield_err = [&](const auto &msg)
    {
        std::cerr << msg << std::endl;
    };

    auto parsed = circ::CmdParser< OptsList >::parse_argv(argc, argv);
    if (!parsed)
    {
        std::cerr << "Command line arguments were not parsed correctly, see "
                  << "stderr for more details.";
        return {};
    }

    auto v = Validator(parsed);

    if (v.check(is_singleton< cli::Help >())
         .check(is_singleton< cli::Version >())
         .process_errors(yield_err))
    {
        return {};
    }

    if (v.check(one_of< input_options >())
         .process_errors(yield_err))
    {
        return {};
    }

    if (v.validate_leaves( OptsList{} ).process_errors(yield_err))
        return {};

    return parsed;
}

int main(int argc, char *argv[]) {
    auto maybe_parsed_cli = parse_and_validate_cli< cmd_opts_list >(argc, argv);
    if (!maybe_parsed_cli)
    {
        std::cerr << circ::help_str(cmd_opts_list());
        return 1;
    }

    auto parsed_cli = std::move(*maybe_parsed_cli);

    if (parsed_cli.template present< circ::cli::Help >())
    {
        std::cout << circ::help_str(cmd_opts_list());
        return 0;
    }

    if (parsed_cli.template present< circ::cli::Version >())
    {
        std::cerr << "TODO: Implement proper version message";
        return 1;
    }

    // NOTE(lukas): Support libraries still need to be initialized, since
    //              remill may be using/relying on them.
    google::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);

    auto circuit = get_input_circuit(parsed_cli);
    if (!circuit)
    {
        circ::unreachable() << "Not able to load circuit.";
        return 3;
    }

    VerifyCircuit("Verifying loaded circuit.", circuit.get(), "Circuit is valid.");



    circ::DefaultOptimizer opt;
    opt.add_pass("remove-transitive-advice");
    opt.add_pass("remove-identity");
    opt.add_pass("remove-trivial-or");
    circuit = opt.run(std::move(circuit));


    if (auto dot_out = parsed_cli.template get< cli::DotOut >()) {
        std::vector< std::string > highlights;
        if (auto input_colors = parsed_cli.template get< cli::DotHighlight >()) {
            highlights = std::move(*input_colors);
        }
        circ::print_circuit(*dot_out, circ::print_dot, circuit.get(),
                             std::unordered_map< circ::Operation *, std::string >{}, highlights);
    }


    if (auto dec_out = parsed_cli.template get< cli:: DecoderOut >()){
        if ( *dec_out != "cout" ) {
            auto o = std::ofstream ( *dec_out );
            auto decGen = circ::decoder::DecoderPrinter( circuit, o );
            decGen.print_file();
    auto seg = circ::circ_to_segg(std::move(circuit));
    std::cout << "Number of starting nodes: "  << seg->_nodes.size() << std::endl;
    circ::dedup(*seg.get());
    std::cout << "dedup nodes: "  << seg->_nodes.size() << std::endl;

    std::map<std::string, circ::Operation*> speciliazes;
    for(auto& node : gap::graph::dfs<gap::graph::yield_node::on_close>(*seg.get()))
    {
        if(node->isRoot)
        {
            for(auto & op : node->roots)
                circ::specialize(speciliazes, node, op.second );
        }
    }

    circ::decoder::ExpressionPrinter ep(std::cout);
    auto max_size_var = circ::decoder::Var("MAX_SIZE_INSTR");
    seg->prepare();
    auto m = seg->get_maximum_vi_size();
    ep.print(circ::decoder::Statement(circ::decoder::Assign(max_size_var, circ::decoder::Int(m))));
    seg->print_semantics_emitter(ep);
    seg->print_decoder(ep);

    /*
     * Emission is two phased:
     *      Phase 1: emit the functions representing the semantics emitter which will get used by the decoder
     *      Phase 2: emit the functions for the decoder
     *
     * The first phase will be walking over the SEG graph and outputting its trees
     * while introducing a function for everything which had the function declartion set
     *
     * The second phase will be going over every VI in the system (the domain of the decoder)
     * then walk the the circIR and corresponding semantics emitter graph in tandem
     * so that the decoder can prefill the stack in a correct manner
     */
    // Print semantics emission functions
    // func decls orders based on hash buckets, need to traverse the graph to keep proper shape of the call graph



//    if (auto dec_out = parsed_cli.template get< cli:: DecoderOut >()){
//        if ( *dec_out != "cout" ) {
//            auto o = std::ofstream ( *dec_out );
//            auto decGen = circ::decoder::DecoderPrinter( circuit, o );
//            decGen.print_file();
//        }
//        else {
//            auto decGen = circ::decoder::DecoderPrinter( circuit );
//            decGen.print_file();
//        }
//    }
//    else{
//        circ::unreachable() << "Decoder out was not specified";
//    }
    circ::semantics::print_semantics(circuit.get());


    return 0;
}
