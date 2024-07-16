#include<vector>
#include<sys/ptrace.h>
#include<sys/wait.h>
#include<unistd.h>
#include<fstream>
#include<sstream>
#include<iostream>
#include <iomanip>
#include<cstring>

#include "linenoise.h"
#include "debugger.hpp"
#include "registers.hpp"

#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>

#include<sys/personality.h>
using namespace minidbg;


std::vector<std::string> split(const std::string &s,char delimiter){
	std::vector<std::string> out{};
	std::stringstream ss {s};
	std::string item;

	while(std::getline(ss,item,delimiter)){
		out.push_back(item);		
	}
	return out;
}

bool is_prefix(const std::string& s,const std::string& of){
	if(s.size()>of.size()) return false;
	return std::equal(s.begin(),s.end(),of.begin());
}

void debugger::continue_execution(){

	step_over_breakpoint();
	ptrace(PTRACE_CONT,m_pid,nullptr,nullptr);
	wait_for_signal();
}

uint64_t debugger::read_memory(uint64_t address){
	return ptrace(PTRACE_PEEKDATA,m_pid,address,nullptr);
}

void debugger::write_memory(uint64_t address,uint64_t value){
	ptrace(PTRACE_POKEDATA,m_pid,address,value);
}

void debugger::dump_registers(){
	for (const auto& rd : g_register_descriptors){
		std::cout << rd.name << "= 0x" << std::setfill('0') << std::setw(10) << std::hex << get_register_value(m_pid,rd.r) << std::endl;
	}
}

uint64_t debugger::get_pc(){
	return get_register_value(m_pid,reg::rip);
}

void debugger::set_pc(uint64_t pc){
	set_register_value(m_pid,reg::rip,pc);
}

void debugger::step_over_breakpoint(){
	
	if(m_breakpoints.count(get_pc())){
		auto& bp=m_breakpoints[get_pc()];
		if(bp.is_enabled()){
			bp.disable();
			ptrace(PTRACE_SINGLESTEP,m_pid,nullptr,nullptr);
			wait_for_signal();
			bp.enable();
		}
	}
}


void debugger::wait_for_signal(){
	int wait_status;
	auto options=0;
	waitpid(m_pid,&wait_status,options);
	
	auto siginfo=get_signal_info();

	switch(siginfo.si_signo){
	case SIGTRAP:
		handle_sigtrap(siginfo);
		break;
	case SIGSEGV:
		std::cout<< "Segfault. Reason: "<<siginfo.si_code<<std::endl;
		break;
	default:
		std::cout<< "Got signal "<<strsignal(siginfo.si_signo) << std::endl;
	}
}

void debugger::handle_sigtrap(siginfo_t info){
	switch(info.si_code){

		case SI_KERNEL:
		case TRAP_BRKPT:
		{
			set_pc(get_pc()-1);
			std::cout << "Hit breakpoint at address 0x" << std::hex << get_pc() <<std::endl;
			auto offset_pc=offset_load_address(get_pc());	

			auto line_entry=get_line_entry_from_pc(offset_pc);
			print_source(line_entry->file->path,line_entry->line);
			return;
			
		}
		case TRAP_TRACE:
			return;
		default:
			std::cout << "Unknown SIGTRAP code " << info.si_code << std::endl;
			return;
	}
}
void debugger::handle_command(const std::string& line){
	auto args=split(line,' ');
	auto command=args[0];

	if(is_prefix(command,"continue")){
		
		continue_execution();

	}else if(is_prefix(command,"break")){
		
		std::string addr {args[1],2};
		set_breakpoint_at_address(std::stol(addr,0,16));

	}else if(is_prefix(command,"register")){
		
		if(is_prefix(args[1],"dump")){
		
			dump_registers();

		}else if (is_prefix(args[1], "read")) {
            		
			std::cout << get_register_value(m_pid, get_register_from_name(args[2])) << std::endl;
        	
		}else if(is_prefix(args[1],"write")){
		
			std::string val {args[3],2};
			set_register_value(m_pid,get_register_from_name(args[2]),std::stol(val,0,16));
		
		}
	}else if(is_prefix(command,"memory")){
		  
		  std::string addr=args[2];
		  if (is_prefix(args[1], "read")) {
			  
			if (args.size() < 4) {

            	            std::cout<< std::hex<< addr << " " << std::hex << read_memory(std::stol(addr, 0, 16)) << std::endl;
        		    return;
			}
        
        		int numWords = std::stoi(args[3]);
        		if (numWords <= 0) {

            			std::cerr << "Error: Number of words must be positive." << std::endl;
           			 return; 
        		}			
        			typedef unsigned int word_type;
                                long address = std::stol(addr, 0, 16);
 
			for (int i = 0; i < numWords; ++i) {
    				std::cout << std::hex << address + i * sizeof(word_type) << " " << read_memory(address + i * sizeof(word_type)) << std::endl;
        		}				  
	  
        	}
		
		  if(is_prefix(args[1],"write")){
			std::string val{args[3],2};
			write_memory(std::stol(addr,0,16),std::stol(val,0,16));
		  }
	}else if(is_prefix(command,"memory_map")){
			print_memory_mappings();

			}else{
		std::cerr<<"Unknown Command\n";
	}

}

void debugger::print_memory_mappings(){
    char command[100];
    snprintf(command, sizeof(command), "/bin/cat /proc/%d/maps",m_pid);

    FILE *fp = popen(command, "r");
    if (fp == nullptr) {
        std::cerr << "Failed to execute command: " << strerror(errno) << std::endl;
        return;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fp) != nullptr) {
        std::cout << buffer;
    }

    pclose(fp);
}

void debugger::intialise_load_address(){
	
	if(m_elf.get_hdr().type==elf::et::dyn){
		
		std::ifstream map("/proc/"+ std::to_string(m_pid) + "/maps");

		std::string addr;
		std::getline(map,addr,'-');

		m_load_address=std::stoi(addr,0,16);
	}
}

uint64_t debugger::offset_load_address(uint64_t addr){
	return addr-m_load_address;
}


siginfo_t debugger::get_signal_info(){
	siginfo_t info;
	ptrace(PTRACE_GETSIGINFO,m_pid,nullptr,&info);
	return info;
}

void debugger::print_source(const std::string& file_name,unsigned line,unsigned n_lines_context){
	std::ifstream file {file_name};

	auto start_line=line<=n_lines_context?1:line-n_lines_context;
	auto end_line=line+n_lines_context+(line < n_lines_context? n_lines_context-line:0)+1;

	char c{};
	auto current_line=1u;

	while(current_line!=start_line && file.get(c)){
		if(c=='\n'){
			++current_line;
		}
	}

	std::cout << (current_line==line? "> ": " ");

	while(current_line<=end_line && file.get(c)){
		std::cout << c;
		if(c=='\n'){
			++current_line;
			std::cout<< (current_line==line?"> ":" ");
		}
	}

	std::cout<<std::endl;
}
void debugger::run(){
	int wait_status;
	auto options=0;
	waitpid(m_pid,&wait_status,options);
	
	intialise_load_address();

	char *line=nullptr;
	while((line=linenoise("dbgr> "))!=nullptr){
		// check if line is not empty
		if(strlen(line)>0){
		handle_command(line);
		linenoiseHistoryAdd(line);
		}
		linenoiseFree(line);

	}	
}

dwarf::die debugger::get_function_from_pc(uint64_t pc){
	for(auto& cu:m_dwarf.compilation_units()){
		if(die_pc_range(cu.root()).contains(pc)){
			for(const auto& die:cu.root()){
				if(die.tag==dwarf::DW_TAG::subprogram){
					if(die_pc_range(die).contains(pc)){
						return die;
					}
				}
			}
		}
	}

	throw std::out_of_range{"Cannot find function"};
}

dwarf::line_table::iterator debugger::get_line_entry_from_pc(uint64_t pc){
	for (auto& cu:m_dwarf.compilation_units()){
		if(die_pc_range(cu.root()).contains(pc)){
			auto &lt=cu.get_line_table();
			auto it=lt.find_address(pc);

			if(it==lt.end()){
				throw std::out_of_range{"Cannot find line entry"};

			}else{
				return it;
			}
		}
	}
	throw std::out_of_range{"Cannot find line entry"};
}
void debugger::set_breakpoint_at_address(std::intptr_t addr){
	std::cout<<"Set breakpoint  at address 0x"<< std::hex << addr << std::endl;
	breakpoint bp{m_pid,addr};
	bp.enable();
	m_breakpoints[addr]=bp;
}

void execute_debugee(const std::string& prog_name){
	if(ptrace(PTRACE_TRACEME,0,0,0)<0){
		std::cerr<<"Error in ptrace\n";
		return;
	}
	execl(prog_name.c_str(),prog_name.c_str(),nullptr);
}
int main(int argc,char * argv[]){
		if(argc<2){
			std::cerr<<"Program not specified";
			return -1;
		}
		auto prog=argv[1];

		auto pid=fork();

		if(pid==0){
			// we are in the child process
			// execute debugee
			personality(ADDR_NO_RANDOMIZE);
			execute_debugee(prog);
		}else if(pid>=1){
			// we are in the parent process
			// execute debugger
			  std::cout << "Started debugging process " << pid << '\n';	
			  debugger dbg{prog, pid};
			  dbg.run();		
		}
	}
