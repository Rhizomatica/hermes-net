/*
 * Mercury: A configurable open-source software-defined modem.
 * Copyright (C) 2023 Fadi Jerji
 * Author: Fadi Jerji
 * Email: fadi.jerji@  <gmail.com, rhizomatica.org, caisresearch.com, ieee.org>
 * ORCID: 0000-0002-2076-5831
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#ifndef INC_FIR_FILTER_H_
#define INC_FIR_FILTER_H_

#include <math.h>
#include <complex.h>
#include <stdlib.h>

#define RECTANGULAR 0
#define HANNING 1
#define HAMMING 2
#define BLACKMAN 3

#define LPF 0

#define SPECTRAL_INVERSION 0


struct st_fir_filter_coefficients
{
	double* filter_coefficients;
	int filter_nTaps;
};



struct st_fir_filter_coefficients* fir_filter_design(int type, int window, double transition_bandwidth, double lpf_cut_frequency, double sampling_frequency);
void fir_filter_apply(struct st_fir_filter_coefficients* coefficients, double _Complex* in, double _Complex* out, int nItems);



#endif
