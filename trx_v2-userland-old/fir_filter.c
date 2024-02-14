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
#include "fir_filter.h"


struct st_fir_filter_coefficients* fir_filter_design(int type, int window, double transition_bandwidth, double lpf_cut_frequency, double sampling_frequency)
{
	struct st_fir_filter_coefficients* fir_filter_coefficients=malloc(sizeof(struct st_fir_filter_coefficients));
	double filter_cut_frequency;

	filter_cut_frequency=lpf_cut_frequency;

	fir_filter_coefficients->filter_nTaps=(int)(4.0/(transition_bandwidth/(sampling_frequency/2.0)));

	if(fir_filter_coefficients->filter_nTaps%2==0)
	{
		fir_filter_coefficients->filter_nTaps++;
	}
	fir_filter_coefficients->filter_coefficients = malloc(sizeof(double)*fir_filter_coefficients->filter_nTaps);
	double sampling_interval=1.0/(sampling_frequency);
	double temp;

	fir_filter_coefficients->filter_coefficients[fir_filter_coefficients->filter_nTaps/2]=1;
	for(int i=0;i<fir_filter_coefficients->filter_nTaps/2;i++)
	{
		temp=2*M_PI*filter_cut_frequency*(double)(fir_filter_coefficients->filter_nTaps/2-i) *sampling_interval;

		fir_filter_coefficients->filter_coefficients[i]=sin(temp)/temp;
		fir_filter_coefficients->filter_coefficients[fir_filter_coefficients->filter_nTaps-i-1]=fir_filter_coefficients->filter_coefficients[i];
	}

	temp=0;
	for(int i=0;i<fir_filter_coefficients->filter_nTaps;i++)
	{
		temp+=fir_filter_coefficients->filter_coefficients[i];
	}

	for(int i=0;i<fir_filter_coefficients->filter_nTaps;i++)
	{
		fir_filter_coefficients->filter_coefficients[i]/=temp;
	}

	if(window==HAMMING)
	{
		for(int i=0;i<fir_filter_coefficients->filter_nTaps;i++)
		{
			fir_filter_coefficients->filter_coefficients[i]*=0.54-0.46*cos(2.0*M_PI*(double)i/(fir_filter_coefficients->filter_nTaps-1));
		}
	}
	else if(window==HANNING)
	{
		for(int i=0;i<fir_filter_coefficients->filter_nTaps;i++)
		{
			fir_filter_coefficients->filter_coefficients[i]*=0.5-0.5*cos(2.0*M_PI*(double)i/(fir_filter_coefficients->filter_nTaps-1));
		}
	}
	else if(window==BLACKMAN)
	{
		for(int i=0;i<fir_filter_coefficients->filter_nTaps;i++)
		{
			fir_filter_coefficients->filter_coefficients[i]*=0.42-0.5*cos(2.0*M_PI*(double)i/fir_filter_coefficients->filter_nTaps)+0.08*cos(4.0*M_PI*(double)i/fir_filter_coefficients->filter_nTaps);
		}
	}

	return fir_filter_coefficients;
}

void fir_filter_apply(struct st_fir_filter_coefficients* coefficients, double _Complex* in, double _Complex* out, int nItems)
{
	double acc_r,acc_im;
	for(int i=0;i<(nItems+coefficients->filter_nTaps-1);i++)
	{
		acc_r=0;
		acc_im=0;
		for(int j=0;j<coefficients->filter_nTaps;j++)
		{
			if((i-j)>=0 && (i-j)<nItems)
			{
				acc_r+=creal(in[i-j])*coefficients->filter_coefficients[j];
				acc_im+=cimag(in[i-j])*coefficients->filter_coefficients[j];
			}
		}

		if(i>=((int)(coefficients->filter_nTaps-1)/2) && i<(nItems+(int)(coefficients->filter_nTaps-1)/2))
		{
			__real__ out[i-(int)(coefficients->filter_nTaps-1)/2]=(acc_r);
			__imag__ out[i-(int)(coefficients->filter_nTaps-1)/2]=(acc_im);
		}
	}
}

