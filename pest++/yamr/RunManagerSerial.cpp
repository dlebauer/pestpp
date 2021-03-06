/*  
	� Copyright 2012, David Welter
	
	This file is part of PEST++.
   
	PEST++ is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	PEST++ is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with PEST++.  If not, see<http://www.gnu.org/licenses/>.
*/
#include "RunManagerSerial.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <iterator>
#include <cassert>
#include <cstring>
#include <map>
#include <algorithm>
#include "system_variables.h"
#include "Transformable.h"
#include "utilities.h"
#include "iopp.h"

#include "network_wrapper.h"

using namespace std;
using namespace pest_utils;


extern "C"
{

	void wrttpl_(int *,
		char *,
		char *,
		int *,
		char *,
		double *,
		int *);

	void readins_(int *,
		char *,
		char *,
		int *,
		char *,
		double *,
		int *);
//mio_initialise(ifail, numin, numout, npar, nobs, precision, decpoint)
	//void mio_initialise_(int *, int *, int *, int *, int *);// , char *, char *);
//mio_put_file(ifail,itype,inum,filename)
	//void mio_put_file_(int *, int *, int *, char *);
//mio_get_file(ifail,itype,inum,filename)
	//void mio_get_file_(int *, int *, int *, char *);
//mio_store_instruction_set(ifail)
	//void mio_store_instruction_set_(int *);
//mio_process_template_files(ifail,npar,apar)
	//void mio_process_template_files_(int *, int *, char *);
//mio_delete_output_files(ifail,asldir)
	//void mio_delete_output_files_(int *, char *);
//mio_write_model_input_files(ifail,npar,apar,pval,asldir)
	//void mio_write_model_input_files_(int *, int *, char *, double *, char *);
//mio_read_model_output_files(ifail,nobs,aobs,obs,instruction,asldir)
	//void mio_read_model_output_files_(int *, int *, char *, double *, char *, char *);
//mio_finalise(ifail)
	//void mio_finalise_(int *);
//mio_get_status(template_status_out,instruction_status_out)
	//void mio_get_status_(int *, int *);
//mio_get_dimensions(numinfile_out,numoutfile_out)
	//void mio_get_dimensions_(int *, int *);
//mio_get_message_string(ifail,amessage_out)
	//void mio_get_message_string_(int *, char *);

}

string RunManagerSerial::tpl_err_msg(int i)
{
	string err_msg;
	switch (i)
	{
	case 0:
		err_msg = "Routine completed successfully";
		break;
	case 1:
		err_msg = "TPL file does not exist";
		break;
	case 2:
		err_msg = "Not all parameters listed in TPL file";
		break;
	case 3:
		err_msg = "Illegal header specified in TPL file";
		break;
	case 4:
		err_msg = "Error getting parameter name from template";
		break;
	case 5:
		err_msg = "Error getting parameter name from template";
		break;
	case 10:
		err_msg = "Error writing to model input file";
	}
	return err_msg;
}


string RunManagerSerial::ins_err_msg(int i)
{
	string err_msg;
	switch (i)
	{
	case 0:
		err_msg = "Routine completed successfully";
		break;
	default:
		err_msg = "";
	}
	return err_msg;
}

RunManagerSerial::RunManagerSerial(const vector<string> _comline_vec,
	const vector<string> _tplfile_vec, const vector<string> _inpfile_vec,
	const vector<string> _insfile_vec, const vector<string> _outfile_vec,
	const string &stor_filename, const string &_run_dir, int _max_run_fail)
	: RunManagerAbstract(_comline_vec, _tplfile_vec, _inpfile_vec,
	_insfile_vec, _outfile_vec, stor_filename, _max_run_fail),
	run_dir(_run_dir)
{
	cout << "              starting serial run manager ..." << endl << endl;
}

void RunManagerSerial::run()
{
	int ifail;
	int success_runs = 0;
	int prev_sucess_runs = 0;
	const vector<string> &par_name_vec = file_stor.get_par_name_vec();
	const vector<string> &obs_name_vec = file_stor.get_obs_name_vec();
	int npar = par_name_vec.size();
	int nobs = obs_name_vec.size();
	int ntpl = tplfile_vec.size();
	int nins = insfile_vec.size();
	//mio_initialise_(&ifail,&ntpl, &nins, &npar, &nobs);
	stringstream message;		
	bool isDouble = true;
	bool forceRadix = true;
	//TemplateFiles tpl_files(isDouble, forceRadix, tplfile_vec, inpfile_vec, par_name_vec);
	//InstructionFiles ins_files(insfile_vec, outfile_vec);
	std::vector<double> obs_vec;
	// This is necessary to support restart as some run many already be complete
	vector<int> run_id_vec;
	int nruns = get_outstanding_run_ids().size();
	while (!(run_id_vec = get_outstanding_run_ids()).empty())
	{
		for (int i_run : run_id_vec)
		{			
			//first delete any existing input and output files	
			// This outer loop is a work around for a bug in windows.  Window can fail to release a file
			// handle quick enough when the external run executes very quickly
			bool failed_file_op = true;
			int n_tries = 0;
			while (failed_file_op)
			{
				vector<string> failed_file_vec;
				failed_file_op = false;
				for (auto &out_file : outfile_vec)
				{
					if ((check_exist_out(out_file)) && (remove(out_file.c_str()) != 0))
					{
						failed_file_vec.push_back(out_file);
						failed_file_op = true;
					}
				}
				for (auto &in_file : inpfile_vec)
				{
					if ((check_exist_out(in_file)) && (remove(in_file.c_str()) != 0))
					{
						failed_file_vec.push_back(in_file);
						failed_file_op = true;
					}
				}
				if (failed_file_op)
				{
					++n_tries;
					w_sleep(1000);
					if (n_tries > 5)
					{
						ostringstream str;
						str << "model interface error: Cannot delete existing following model files:";
						for (const string &ifile : failed_file_vec)
						{
							str << " " << ifile;
						}
						throw PestError(str.str());
					}
				}

			}
			Observations obs;
			vector<double> par_values;
			Parameters pars;
			file_stor.get_parameters(i_run, pars);						
			try {
				std::cout << string(message.str().size(), '\b');
				message.str("");
				message << "(" << success_runs << "/" << nruns << " runs complete)";
				std::cout << message.str();
				OperSys::chdir(run_dir.c_str());
				for (auto &i : par_name_vec)
				{
					par_values.push_back(pars.get_rec(i));
				}
				if (std::any_of(par_values.begin(), par_values.end(), OperSys::double_is_invalid))
				{
					throw PestError("Error running model: invalid parameter value returned");
				}
				wrttpl_(&ntpl, StringvecFortranCharArray(tplfile_vec, 50).get_prt(),
					StringvecFortranCharArray(inpfile_vec, 50).get_prt(),
					&npar, StringvecFortranCharArray(par_name_vec, 50, pest_utils::TO_LOWER).get_prt(),
					&par_values[0], &ifail);
				if (ifail != 0)
				{
					throw PestError("Error processing template file");
				}								
				for (int i = 0, n_exec = comline_vec.size(); i < n_exec; ++i)
				{
					system(comline_vec[i].c_str());
				}
				obs_vec.resize(nobs, RunStorage::no_data);
				readins_(&nins, StringvecFortranCharArray(insfile_vec, 50).get_prt(),
					StringvecFortranCharArray(outfile_vec, 50).get_prt(),
					&nobs, StringvecFortranCharArray(obs_name_vec, 50, pest_utils::TO_LOWER).get_prt(),
					&obs_vec[0], &ifail);
				if (ifail != 0)
				{
					throw PestError("Error processing instruction file");
				}				
				// check parameters and observations for inf and nan
				if (std::any_of(par_values.begin(), par_values.end(), OperSys::double_is_invalid))
				{
					throw PestError("Error running model: invalid parameter value returned");
				}
				if (std::any_of(obs_vec.begin(), obs_vec.end(), OperSys::double_is_invalid))
				{
					throw PestError("Error running model: invalid observation value returned");
				}

				/*
				// IOPP version
				for (int i = 0, n_exec = comline_vec.size(); i < n_exec; ++i)
				{
				system(comline_vec[i].c_str());
				}
				//pest_utils::thread_flag* tf1(false);
				//pest_utils::thread_flag* tf2(false);
				//w_run_commands(tf1,tf2,comline_vec);
				ins_files.read(obs_name_vec, obs);
				*/

				success_runs += 1;
				
				pars.clear();
				pars.insert(par_name_vec, par_values);
				obs.clear();
				obs.insert(obs_name_vec, obs_vec);								
				file_stor.update_run(i_run, pars, obs);

			}
			catch (const std::exception& ex)
			{
				update_run_failed(i_run);
				cerr << endl;
				cerr << "  " << ex.what() << endl;
				cerr << "  Aborting model run" << endl << endl;
			}
			catch (...)
			{
				update_run_failed(i_run);
				cerr << endl;
				cerr << "  Error running model" << endl;
				cerr << "  Aborting model run" << endl << endl;
			}
		}
	}

	total_runs += success_runs;
	std::cout << string(message.str().size(), '\b');
	message.str("");
	message << "(" << success_runs << "/" << nruns << " runs complete";
	if (prev_sucess_runs > 0)
	{
		message << " and " << prev_sucess_runs << " additional run completed previously";
	}
	message << ")";
	std::cout << message.str();
	if (success_runs < nruns)
	{			cout << endl << endl;
		cout << "WARNING: " << nruns - success_runs << " out of " << nruns << " runs failed" << endl << endl;
	}
	std::cout << endl << endl;
	/*if (init_run_obs.size() == 0)
		int status = file_stor.get_observations(0, init_run_obs);*/
	if (init_sim.size() == 0)
	{
		vector<double> pars;
		int status = file_stor.get_run(0, pars, init_sim);
	}
}


RunManagerSerial::~RunManagerSerial(void)
{
}
