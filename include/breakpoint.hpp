#ifndef MINIDBG_BREAKPOINT_HPP
#define MINIDBG_BREAKPOINT_HPP

#include<cstdint>
#include<sys/ptrace.h>

namespace minidbg{
	class breakpoint{
		public:
			breakpoint()=default;
			breakpoint(pid_t pid,std::intptr_t addr) : m_pid{pid},m_addr{addr},m_enabled{false},m_saved_data{}
			{}

			void enable(){
				// read the memory of the traced proccess
				auto data=ptrace(PTRACE_PEEKDATA,m_pid,m_addr,nullptr);
				// save the bottom byte
				m_saved_data=static_cast<uint8_t>(data & 0xff);
				uint64_t int3=0xcc;
				// set bottom byte to 0xcc
				uint64_t data_with_int3=((data& ~0xff)|int3);
				// set the breakpoint by overwriting that part of memory
				ptrace(PTRACE_POKEDATA,m_pid,m_addr,data_with_int3);

				m_enabled=true;
				
			}
			void disable(){
			// read the memory of the traced process
			auto data=ptrace(PTRACE_PEEKDATA,m_pid,m_addr,nullptr);
			// remove int3
			auto restored_data=((data & ~0xff)|m_saved_data);
			// copy the word restored_data to m_addr
			ptrace(PTRACE_POKEDATA,m_pid,m_addr,restored_data);

			m_enabled=false;
			}

			auto is_enabled() const -> bool {return m_enabled;}
			auto get_address() const -> std::intptr_t {return m_addr;}
		private:
			pid_t m_pid;
			std::intptr_t m_addr;
			bool m_enabled;
			uint8_t m_saved_data;
	};
}

#endif
