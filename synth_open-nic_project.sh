cd open-nic-shell/script/

vivado -mode tcl -source build.tcl -tclargs -board au250 -jobs 8 -synth_ip 1 -impl 1 -use_phys_func 1 -sim 0