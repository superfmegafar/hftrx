%% ================================================================================
%% Legal Notice: Copyright (C) 1991-2008 Altera Corporation
%% Any megafunction design, and related net list (encrypted or decrypted),
%% support information, device programming or simulation file, and any other
%% associated documentation or information provided by Altera or a partner
%% under Altera's Megafunction Partnership Program may be used only to
%% program PLD devices (but not masked PLD devices) from Altera.  Any other
%% use of such megafunction design, net list, support information, device
%% programming or simulation file, or any other related documentation or
%% information is prohibited for any other purpose, including, but not
%% limited to modification, reverse engineering, de-compiling, or use with
%% any other silicon devices, unless such use is explicitly licensed under
%% a separate agreement with Altera or a megafunction partner.  Title to
%% the intellectual property, including patents, copyrights, trademarks,
%% trade secrets, or maskworks, embodied in any such megafunction design,
%% net list, support information, device programming or simulation file, or
%% any other related documentation or information provided by Altera or a
%% megafunction partner, remains with Altera, the megafunction partner, or
%% their respective licensors.  No other licenses, including any licenses
%% needed under any third party's intellectual property, are provided herein.
%% ================================================================================
%%
%% Generated by: CIC 13.1 Build 182 October, 2013
%% Generated on: Jul 3, 2020 12:46:36 AM
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
% cic_128a_2iq_fir_comp_coeff genearats CIC compensation filter coefficients
% using frequency sampling method.
%
%    cic_128a_2iq_fir_comp_coeff(L, Fs, Fc, plot, is_fxp, B) calculates compensation
%    filter coefficients and saves the coefficients to a file.
%
%    L      - FIR filter length (= number of taps = number of coefficients)
%    Fs     - FIR filter sample rate in Hz before decimation
%    Fc     - FIR filter cutoff frequency in Hz
%    plot   - True or false to draw filter responses graphically
%    is_fxp - Indicating if the coefficients should be saved as fixed point or floating point numbers
%    B      - Number of bits to represent the coefficients if is_fxp is true
%
%    Examples:
%    cic_128a_2iq_fir_comp_coeff(31,80e6,4e6,true,true,16);
%    cic_128a_2iq_fir_comp_coeff(31,80e6,4e6,true,false)


function cic_128a_2iq_fir_comp_coeff(L, Fs, Fc, plot, is_fxp, B)
try
	% Validate number of input arguments
	error(nargchk(0, 6, nargin));

	%%%%%% CIC filter parameters %%%%%%
	R = 128;					%% Decimation factor
	M = 1;					%% Differential Delay
	N = 9;					%% Number of Stages
	
	%%%%%% User Parameters %%%%%%
	% Check if Signal Processing Toolbox is in the Matlab installation
	if (isempty(which('fir2')))
		error('Matlab Signal Processing Toolbox isn''t installed on your machine. Please contact Altera for help.');
	end
	
	%% Get user parameters: B, L, Fs, Fc if they aren't passed as arguments.
	if(nargin < 1)
	    %% Filter length: it must be an odd number, otherwise it'll be increased by 1.
		L = input('Number of filter coefficients (31 as default): ');
		if isempty(L)
			L = 31;                             
		end
    end
	if mod(L,2) == 0
		fprintf('FIR filter length must be an odd number. %d is used instead.\n',L+1);
		L = L+1;
	end
    
    
	if(nargin < 2)
	    %% FIR filter sample rate in Hz before decimation
		Fs = input('FIR filter sample rate in Hz before decimation (80e6 as default): ');
		if isempty(Fs)
	    	Fs = 80e6;
	    end
	end
		
	if(nargin < 3)
	    %% FIR filter cutoff frequency in Hz
		Fc = input('FIR filter cutoff frequency in Hz (4e6 as default): ');
		if isempty(Fc)
			Fc = 4e6;
	    end
    end
    
    if (nargin < 4)
    	plot_res = input('Do you want to plot filter responses? Y/N [N]:','s');
		if isempty(plot_res)
			plot_res = 'N';
		end
		if upper(plot_res) == 'Y'
			plot = true;
		else
			plot = false;
		end
    end
    
	if(nargin < 5)
		fxp_coeff = input('Do you want to write out coefficients as fixed point numbers? Y/N [Y]:','s');
		if isempty(fxp_coeff)
			fxp_coeff = 'Y';
		end
		if upper(fxp_coeff) == 'Y'
			is_fxp = true;
		else
			is_fxp = false;
		end
    end
	
	if(is_fxp && nargin < 6)
		%% Number of bits to represent fixed point filter coefficients
		B = input('Number of bits to represent the filter coefficients (16 as default): ');
		if isempty(B)
	    	B = 16;
	    end
	end
		
	Fo = R*Fc/Fs;                            %% Normalized Cutoff freq; 0<Fo<=0.5/M; 
	                                         %% Fo should be less than 1/(4M) for good performance                              
	% Fo = 0.5/M;                            %% use Fo=0.5 if we don't care responses outside passband
	
	%%%%%%% CIC Compensator Design using fir2.m %%%%%%
	p = 2e3;                                 %% Granulatiry
	s = 0.25/p;                              %% Stepsize
	fp = [0:s:Fo];                           %% Passband frequency samples                                        
	fs = (Fo+s):s:0.5;                       %% Stopband frequency samples
	f = [fp fs]*2;                           %% Noramlized frequency samples; 0<=f<=1; 
	Mp = ones(1,length(fp));                 %% Passband response; Mp(1)=1
	Mp(2:end) = abs( M*R*sin(pi*fp(2:end)/R)./sin(pi*M*fp(2:end))).^N; %% Inverse sinc
	Mf = [Mp zeros(1,length(fs))];
	f(end) = 1;
	h = fir2(L-1,f,Mf);                      %% Filter order = filter length (L) - 1
	h = h/max(h);                            %% Floating point coefficients, scaled it to 1
	
	%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	% Output filter coefficients to a file for Altera FIR Compiler           %
	%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	
	filename = 'cic_128a_2iq_fir_comp_coeff.txt';
	fid = fopen(filename, 'wt');
	if (fid == -1)
		errMsg = sprintf('Can''t Open file %s for writing.\n', FileName);
		error(errMsg);
	end;
	
	if (is_fxp) % coefficients are fixed point
		hz = fix(h*(2^(B-1)-1));               %% Quantization of filter coefficients
		fprintf(fid, '%d\n', hz); %% fixed point coeff
	else
		fprintf(fid, '%.6f\n',h); %% floating point
	end
	fclose(fid);
	
	fprintf('The compensation filter coefficients have been saved to file ''%s''.\n',filename);
	
	% Change the following variable to 1 if you would like to plot the filter responses
	
	if (plot)
		if (is_fxp)
			plot_responses(R,M,N,hz,Fs,is_fxp);
		else
			plot_responses(R,M,N,h,Fs,is_fxp);
		end
	end;
catch
	rethrow(lasterror);
end;

%% Function for ploting filter responses
function plot_responses(R,M,N,res,Fs,is_fxp)
try
	%%%%%%% Full resolution CIC filter response %%%%%%%%
	hrec = ones(1,R*M);
	tmph = hrec;
	
	for k=1:N-1
	    tmph = conv(hrec, tmph);
	end;
	hcic = tmph;
	hcic = hcic/max(hcic);
	
	%%%%%%% Total Response %%%%%%%%%%%%%%%
	resp = upsample(res, R);
	ht = conv(hcic, resp);              %% Concatenation of CIC and fir2 FIR at high freqency
	
	no_data_points = 4096;              %% Number of data points to plot in the figure
	
	[Hcic, wt] = freqz(hcic, 1, no_data_points, Fs);      %% CIC Freq. Response
	[Hciccomp, wt] = freqz(resp, 1, no_data_points, Fs);  %% CIC Comp. response using fir2
	[Ht, wt] = freqz(ht, 1, no_data_points, Fs);          %% Total response for CIC + Compensation fir2
	
	warning off all;                                      %% Turn the 'Log by zero' warning off
	Mcic = 20*log10(abs(Hcic)/(abs(Hcic(1))));            %% CIC Freq. Response
	Mciccomp = 20*log10(abs(Hciccomp)/(abs(Hciccomp(1))));%% CIC Comp. response using fir2
	Mt = 20*log10(abs(Ht)/(abs(Ht(1))));                  %% Total response for CIC + Compensation fir2
	warning on;                                           %% Turn warnings on
	
	figure;
	plot(wt, Mcic, wt, Mciccomp, wt, Mt);
	if (is_fxp) % coefficients are fixed point
		legend('CIC','CIC Comp','Total Response (Fixed Point)');
	else % floating point
		legend('CIC','CIC Comp','Total Response (Floating Point)');
	end
	ymax = max(Mciccomp)+1;
	ylim([-100 ymax]);
	title('CIC and its Compensation Filter Responses');
	grid;
	xlabel('Frequency Hz');
	ylabel('Filter Magnitude Response dB');
catch
	rethrow(lasterror);
end;

