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
#include <iostream>
#include "plot.h"
#include "fft.h"
#include <unistd.h>
#include <cmath>

extern "C"
{
#include "fir_filter.h"
#include "interpolation.h"
}



int main()
{





	cl_plot plot1, plot2, plot3;

	plot1.open("test1");
	plot1.reset("test1");

	plot2.open("test2");
	plot2.reset("test2");

	plot3.open("test3");
	plot3.reset("test3");



	float data_plot[(int)Nfft][2];


	std::complex <double> passband_data_ifft[(int)Nfft];
	std::complex <double> passband_data_fft[(int)Nfft];
	std::complex <double> passband_data[(int)Nfft];

	std::complex <double> intermediate_data[(int)Nfft];
	std::complex <double> intermediate_data_fft[(int)Nfft];

	std::complex <double> passband_data_tx_fft[(int)Nfft];
	std::complex <double> passband_data_tx[(int)Nfft];

	double c_passband_data[(int)Nfft];

	double _Complex c_baseband_data[(int)Nfft];
	double _Complex c_baseband_data_filtered[(int)Nfft];

	double c_intermediate_data[(int)Nfft];
	double c_intermediate_data_decimated[(int)Nfft/2];
	double c_intermediate_data_interpolated[(int)Nfft];

	double c_passband_data_tx[(int)Nfft];


	int nPoints=Nfft;

	for(int i=0;i<nPoints;i++)
	{
		passband_data_ifft[i]=0;
	}
	for(int i=4;i<224;i++) // generate test signal (375Hz, 21Khz) @96000
	{
		passband_data_ifft[i]=std::complex <double>(1,0);
		passband_data_ifft[Nfft-i-1]=passband_data_ifft[i];

	}
//	passband_data_ifft[170]=std::complex <double>(5,0);
//	passband_data_ifft[180]=std::complex <double>(0.1,0);

	ifft(passband_data_ifft,passband_data);


	for(int i=0;i<nPoints;i++)
	{
		c_passband_data[i]=passband_data[i].real();
	}


	/////////////////// RX C DSP code start//////////////////

	double carrier_amplitude=sqrt(2);

	double passband_samp_freq=96000;
	double passband_start_freq=16000;
	double passband_end_freq=19000;

	double intermediate_samp_freq=48000;
	double intermediate_carrier_frequency=1000+(passband_end_freq-passband_start_freq)/2;

	double passband_samp_interval=1.0/passband_samp_freq;
	double passband_carrier_frequency=(passband_start_freq+passband_end_freq)/2.0;

	struct st_fir_filter_coefficients* FIR_filter= fir_filter_design(LPF, BLACKMAN, 100, intermediate_carrier_frequency, passband_samp_freq);

	for(int i=0;i<nPoints;i++)
	{
		__real__ c_baseband_data[i] =c_passband_data[i]*carrier_amplitude*cos(2*M_PI*(passband_carrier_frequency)*(double)i * passband_samp_interval);
		__imag__ c_baseband_data[i] =c_passband_data[i]*carrier_amplitude*sin(2*M_PI*(passband_carrier_frequency)*(double)i * passband_samp_interval);
	}

	fir_filter_apply(FIR_filter, c_baseband_data,c_baseband_data_filtered, nPoints);

	for(int i=0;i<nPoints;i++)
	{
		c_intermediate_data[i] =creal(c_baseband_data_filtered[i])*carrier_amplitude*cos(2*M_PI*(intermediate_carrier_frequency)*(double)i * passband_samp_interval);
		c_intermediate_data[i] +=cimag(c_baseband_data_filtered[i])*carrier_amplitude*sin(2*M_PI*(intermediate_carrier_frequency)*(double)i * passband_samp_interval);
	}

	rational_resampler(c_intermediate_data,nPoints,c_intermediate_data_decimated,(int)(passband_samp_freq/intermediate_samp_freq),DECIMATION);

	/////////////////// RX C DSP code end //////////////////

	/////////////////// TX C DSP code start//////////////////

	rational_resampler(c_intermediate_data_decimated,(int)((double)(nPoints)*intermediate_samp_freq/passband_samp_freq),c_intermediate_data_interpolated,(int)(passband_samp_freq/intermediate_samp_freq),INTERPOLATION);

	for(int i=0;i<nPoints;i++)
	{
		__real__ c_baseband_data[i] =c_intermediate_data_interpolated[i]*carrier_amplitude*cos(2*M_PI*(intermediate_carrier_frequency)*(double)i * passband_samp_interval);
		__imag__ c_baseband_data[i] =c_intermediate_data_interpolated[i]*carrier_amplitude*sin(2*M_PI*(intermediate_carrier_frequency)*(double)i * passband_samp_interval);
	}

	fir_filter_apply(FIR_filter, c_baseband_data,c_baseband_data_filtered, nPoints);

	for(int i=0;i<nPoints;i++)
	{
		c_passband_data_tx[i] =creal(c_baseband_data_filtered[i])*carrier_amplitude*cos(2*M_PI*(passband_carrier_frequency)*(double)i * passband_samp_interval);
		c_passband_data_tx[i] +=cimag(c_baseband_data_filtered[i])*carrier_amplitude*sin(2*M_PI*(passband_carrier_frequency)*(double)i * passband_samp_interval);
	}


	/////////////////// TX C DSP code end //////////////////


	for(int i=0;i<nPoints;i++)
	{
		passband_data[i].real(c_passband_data[i]);
		passband_data[i].imag(0);
	}
	fft(passband_data,passband_data_fft);

	for(int i=0;i<nPoints;i++)
	{
		data_plot[i][0]=(float)i;

		if(i<nPoints/2)
		{
			data_plot[i+nPoints/2][1]=(double)10*log10(passband_data_fft[i].real());
		}
		else
		{
			data_plot[i-nPoints/2][1]=(double)10*log10(passband_data_fft[i].real());
		}
	}

	plot1.plot("passband_data_fft db",&data_plot[0][0],nPoints);



	for(int i=0;i<nPoints;i++)
	{
		intermediate_data[i].real(c_intermediate_data[i]);
		intermediate_data[i].imag(0);
	}
	fft(intermediate_data,intermediate_data_fft);

	for(int i=0;i<nPoints;i++)
	{
		data_plot[i][0]=(float)i;
		if(i<nPoints/2)
		{
			data_plot[i+nPoints/2][1]=(double)10*log10(intermediate_data_fft[i].real());
		}
		else
		{
			data_plot[i-nPoints/2][1]=(double)10*log10(intermediate_data_fft[i].real());
		}
	}

	plot2.plot("intermediate_data_fft (db)",&data_plot[0][0],nPoints);



	for(int i=0;i<nPoints;i++)
	{
		passband_data_tx[i].real(c_passband_data_tx[i]);
		passband_data_tx[i].imag(0);
	}
	fft(passband_data_tx,passband_data_tx_fft);

	for(int i=0;i<nPoints;i++)
	{
		data_plot[i][0]=(float)i;
		if(i<nPoints/2)
		{
			data_plot[i+nPoints/2][1]=(double)10*log10(passband_data_tx_fft[i].real());
		}
		else
		{
			data_plot[i-nPoints/2][1]=(double)10*log10(passband_data_tx_fft[i].real());
		}
	}

	plot3.plot("passband_data_tx_fft (db)",&data_plot[0][0],nPoints);


	plot1.close();
	plot2.close();
	plot3.close();

	return 0;
}

