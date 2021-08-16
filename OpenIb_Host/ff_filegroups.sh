basic_tools_sbin="ethcapture"

basic_tools_sbin_sym=""

basic_tools_opt="setup_self_ssh usemem ethipcalc stream"

basic_mans="ethcapture.1"

basic_configs="mgt_config.xml"

basic_samples="mgt_config.xml-sample"

ff_tools_opt=""

ff_tools_exp="basic.exp common_funcs.exp ff_function.exp
	ib.exp eth_to_xml.exp ibtools.exp install.exp load.exp mpi.exp
	mpiperf.exp mpiperfdeviation.exp network.exp proc_mgr.exp reboot.exp rping.exp
	target.exp tools.exp upgrade.exp tclIndex tcl_proc comm12 front"

ff_tools_sbin="ethcabletest ethcheckload ethextracterror ethextractlink
	ethextractperf ethextractstat ethextractstat2 ethfindgood
	ethlinkanalysis ethreport ethsorthosts ethxlattopology ethxmlextract
	ethxmlfilter ethxmlgenerate ethxmlindent ethallanalysis ethcaptureall
	ethcmdall ethdownloadall ethfabricanalysis
	ethfastfabric ethexpandfile ethextractbadlinks ethextractifids ethextractsellinks
	ethextractmissinglinks ethverifyhosts ethhostadmin
	ethpingall ethscpall ethsetupsnmp ethsetupssh ethshowallports ethfabricinfo
	ethuploadall eth2rm ethextractperf2 ethmergeperf2"

ff_tools_misc="ff_funcs ethgetipaddrtype ethfastfabric.conf.def show_counts"

ff_tools_fm=""

ff_libs_misc="libqlgc_fork.so"

ff_mans="ethallanalysis.8 ethcabletest.8 ethcaptureall.8
	ethcheckload.8 ethcmdall.8 ethdownloadall.8
	ethexpandfile.8 ethextractbadlinks.8 ethextracterror.8
	ethextractifids.8 ethextractlink.8 ethextractperf.8 ethextractsellinks.8
	ethextractstat.8 ethextractstat2.8 ethfabricanalysis.8 ethfastfabric.8
	ethfindgood.8 ethgentopology.8 ethhostadmin.8 ethlinkanalysis.8 ethpingall.8
	ethreport.8 ethscpall.8 ethsetupsnmp.8 ethsetupssh.8 ethshowallports.8 ethsorthosts.8
	ethuploadall.8 ethverifyhosts.8 ethxlattopology.8
	ethxmlextract.8 ethxmlfilter.8 ethextractperf2.8 ethmergeperf2.8
	ethxmlgenerate.8 ethxmlindent.8 ethextractmissinglinks.8 eth2rm.8 ethfabricinfo.8"

ff_iba_samples="hostverify.sh ethtopology_NICs.txt ethtopology_links.txt
	ethtopology_SWs.txt linksum_swd06.csv linksum_swd24.csv README.topology
	README.xlat_topology minimal_topology.xlsx detailed_topology.xlsx allhosts-sample
	hosts-sample switches-sample mac_to_dhcp
	ethmon.conf-sample ethmon.si.conf-sample ethfastfabric.conf-sample
	ethgentopology"

help_doc=""

mpi_apps_files="Makefile mpi_hosts.sample README prepare_run select_mpi run_bw
	get_selected_mpi.sh get_mpi_blas.sh *.params gen_group_hosts gen_mpi_hosts
	mpi_cleanup stop_daemons hpl_dat_gen config_hpl2 run_hpl2 run_lat run_imb 
	run_app runmyapp mpicheck run_mpicheck run_deviation
	run_multibw run_mpi_stress run_osu5 run_cabletest run_allniclatency
	run_alltoall5 run_bcast5 run_bibw5 run_bw5 run_lat5 run_mbw_mr5 run_multi_lat5
	run_batch_script run_batch_cabletest hpl-count.diff
	groupstress deviation
	hpl-config/HPL.dat-* hpl-config/README mpicc mpif77 mpicxx"

