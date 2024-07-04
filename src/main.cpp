#include<vector>
#include<sys/ptrace.h>
#include<sys/wait.h>
#include<unistd.h>
#include<sstream>
#include<iostream>
#include<cstring>
#include "linenoise.h"
#include "debugger.hpp"

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
	ptrace(PTRACE_CONT,m_pid,nullptr,nullptr);

	int wait_status;
	auto options=0;
	waitpid(m_pid,&wait_status,options);
}
void debugger::handle_command(const std::string& line){
	auto args=split(line,' ');
	auto command=args[0];

	if(is_prefix(command,"continue")){
		continue_execution();
	}else{
		std::cerr<<"Unknown Command\n";
	}

}
void debugger::run(){
	int wait_status;
	auto options=0;
	waitpid(m_pid,&wait_status,options);

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
		}else if(pid>=1){
			// we are in the parent process
			// execute debugger
			  std::cout << "Started debugging process " << pid << '\n';
    				debugger dbg{prog, pid};
			    	dbg.run();		
		}
	}
