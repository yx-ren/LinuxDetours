make -j4 2> error && ./dump_obj.sh trampoline_mips.cpp.o && ./gen_ins.sh trampoline_mips.cpp.o.obj.dump && ./replace_hard_code.sh detours.cpp && make -j4
