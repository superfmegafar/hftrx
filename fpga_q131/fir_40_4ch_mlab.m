%                                                                                                          
%THIS IS A WIZARD GENERATED FILE. DO NOT EDIT THIS FILE!                                                   
%                                                                                                          
%--------------------------------------------------------------------------------------------------------- 
%This Model Only Support Single Channel Input Data. 
%Please input:                                      
%data vector: 		stimulation(1:n)                
%FILTER PARAMETER  
%Input Data Width: 32
%Decimation Factor: 40
%MSB Truncated Bits:
%MSB Satuated Bits :
%LSB Truncated bits:
%LSB Rounded bits  :
%FIR Width (Full Calculation Width Before Output Width Adjust) :32 
%-----------------------------------------------------------------------------------------------------------
%MegaWizard Scaled Coefficient Values
 function  output = fir_40_4ch_mlab(stimulation, output);
 coef_matrix=[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-18,-22,-27,-31,-36,-41,-47,-53,-59,-65,-71,-78,-85,-92,-99,-106,-113,-120,-127,-134,-141,-147,-153,-159,-164,-169,-174,-178,-181,-184,-186,-187,-187,-187,-185,-182,-179,-174,-168,-161,-153,-143,-133,-121,-107,-92,-76,-59,-40,-20,0,23,47,72,98,125,153,182,212,242,274,306,338,371,404,437,469,502,535,567,598,629,658,687,714,740,764,786,806,824,840,854,864,872,877,879,878,873,865,853,837,818,795,767,736,701,661,618,570,518,462,402,338,270,199,124,45,-37,-122,-210,-301,-395,-490,-588,-687,-788,-889,-992,-1094,-1197,-1299,-1401,-1501,-1600,-1697,-1791,-1882,-1971,-2055,-2135,-2210,-2281,-2346,-2405,-2457,-2503,-2542,-2573,-2596,-2611,-2617,-2614,-2602,-2581,-2549,-2508,-2457,-2395,-2323,-2241,-2148,-2044,-1930,-1805,-1670,-1525,-1370,-1204,-1030,-846,-653,-452,-243,-26,198,428,664,905,1151,1400,1653,1907,2164,2420,2677,2932,3185,3435,3680,3921,4156,4383,4603,4814,5014,5204,5381,5546,5697,5832,5953,6056,6142,6210,6259,6288,6297,6285,6251,6195,6117,6016,5892,5745,5574,5380,5162,4920,4656,4368,4058,3725,3371,2996,2601,2185,1752,1300,832,348,-150,-661,-1184,-1717,-2258,-2807,-3361,-3918,-4477,-5037,-5594,-6148,-6696,-7236,-7766,-8285,-8790,-9278,-9749,-10200,-10628,-11033,-11411,-11762,-12083,-12372,-12627,-12847,-13031,-13176,-13281,-13345,-13367,-13345,-13279,-13167,-13008,-12803,-12551,-12250,-11902,-11506,-11061,-10569,-10031,-9445,-8814,-8138,-7418,-6656,-5852,-5010,-4130,-3215,-2266,-1286,-278,756,1814,2892,3987,5096,6216,7343,8474,9606,10734,11855,12964,14059,15135,16187,17213,18208,19168,20090,20968,21799,22580,23307,23975,24582,25123,25596,25997,26323,26571,26739,26824,26823,26735,26558,26290,25929,25474,24926,24282,23543,22708,21779,20754,19637,18427,17126,15736,14260,12699,11056,9336,7540,5674,3740,1744,-310,-2417,-4571,-6768,-9000,-11263,-13549,-15853,-18167,-20484,-22798,-25100,-27384,-29641,-31865,-34046,-36178,-38252,-40260,-42193,-44045,-45806,-47470,-49027,-50470,-51792,-52984,-54040,-54951,-55712,-56314,-56752,-57019,-57108,-57016,-56735,-56260,-55588,-54713,-53632,-52342,-50838,-49119,-47182,-45025,-42648,-40049,-37228,-34186,-30923,-27440,-23740,-19824,-15695,-11358,-6814,-2070,2871,8002,13318,18813,24479,30309,36295,42430,48704,55108,61633,68268,75004,81831,88736,95709,102739,109813,116921,124049,131185,138317,145433,152520,159565,166555,173478,180322,187071,193716,200244,206643,212901,219003,224942,230704,236279,241654,246821,251769,256490,260973,265209,269190,272910,276359,279532,282423,285025,287332,289342,291049,292450,293543,294326,294796,294952,294796,294326,293543,292450,291049,289342,287332,285025,282423,279532,276359,272910,269190,265209,260973,256490,251769,246821,241654,236279,230704,224942,219003,212901,206643,200244,193716,187071,180322,173478,166555,159565,152520,145433,138317,131185,124049,116921,109813,102739,95709,88736,81831,75004,68268,61633,55108,48704,42430,36295,30309,24479,18813,13318,8002,2871,-2070,-6814,-11358,-15695,-19824,-23740,-27440,-30923,-34186,-37228,-40049,-42648,-45025,-47182,-49119,-50838,-52342,-53632,-54713,-55588,-56260,-56735,-57016,-57108,-57019,-56752,-56314,-55712,-54951,-54040,-52984,-51792,-50470,-49027,-47470,-45806,-44045,-42193,-40260,-38252,-36178,-34046,-31865,-29641,-27384,-25100,-22798,-20484,-18167,-15853,-13549,-11263,-9000,-6768,-4571,-2417,-310,1744,3740,5674,7540,9336,11056,12699,14260,15736,17126,18427,19637,20754,21779,22708,23543,24282,24926,25474,25929,26290,26558,26735,26823,26824,26739,26571,26323,25997,25596,25123,24582,23975,23307,22580,21799,20968,20090,19168,18208,17213,16187,15135,14059,12964,11855,10734,9606,8474,7343,6216,5096,3987,2892,1814,756,-278,-1286,-2266,-3215,-4130,-5010,-5852,-6656,-7418,-8138,-8814,-9445,-10031,-10569,-11061,-11506,-11902,-12250,-12551,-12803,-13008,-13167,-13279,-13345,-13367,-13345,-13281,-13176,-13031,-12847,-12627,-12372,-12083,-11762,-11411,-11033,-10628,-10200,-9749,-9278,-8790,-8285,-7766,-7236,-6696,-6148,-5594,-5037,-4477,-3918,-3361,-2807,-2258,-1717,-1184,-661,-150,348,832,1300,1752,2185,2601,2996,3371,3725,4058,4368,4656,4920,5162,5380,5574,5745,5892,6016,6117,6195,6251,6285,6297,6288,6259,6210,6142,6056,5953,5832,5697,5546,5381,5204,5014,4814,4603,4383,4156,3921,3680,3435,3185,2932,2677,2420,2164,1907,1653,1400,1151,905,664,428,198,-26,-243,-452,-653,-846,-1030,-1204,-1370,-1525,-1670,-1805,-1930,-2044,-2148,-2241,-2323,-2395,-2457,-2508,-2549,-2581,-2602,-2614,-2617,-2611,-2596,-2573,-2542,-2503,-2457,-2405,-2346,-2281,-2210,-2135,-2055,-1971,-1882,-1791,-1697,-1600,-1501,-1401,-1299,-1197,-1094,-992,-889,-788,-687,-588,-490,-395,-301,-210,-122,-37,45,124,199,270,338,402,462,518,570,618,661,701,736,767,795,818,837,853,865,873,878,879,877,872,864,854,840,824,806,786,764,740,714,687,658,629,598,567,535,502,469,437,404,371,338,306,274,242,212,182,153,125,98,72,47,23,0,-20,-40,-59,-76,-92,-107,-121,-133,-143,-153,-161,-168,-174,-179,-182,-185,-187,-187,-187,-186,-184,-181,-178,-174,-169,-164,-159,-153,-147,-141,-134,-127,-120,-113,-106,-99,-92,-85,-78,-71,-65,-59,-53,-47,-41,-36,-31,-27,-22,-18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
 INTER_FACTOR  = 1;         
 DECI_FACTOR  =  40;
 MSB_RM  = 1;
 MSB_TYPE  = 0;
 LSB_RM  = 24;
 LSB_TYPE  = 0;
 FIR_WIDTH  = 32 + MSB_RM + LSB_RM;
 OUT_WIDTH  = 32 ;  %32
  DATA_WIDTH = 32;
 data_type = 1;

 % check size of inputs. 
 [DX,DY] = size(stimulation);
 [CX,CY] = size(coef_matrix); 
 if (CX ~= DY * INTER_FACTOR) 
    fprintf('WARNING : coef_matrix size and input data size is not match\n'); 
 end 
   
 %fill coef_matrix to length of data with the latest coef set 
 if (CX < DY * INTER_FACTOR) 
   for i= CX +1:DY * INTER_FACTOR 
     coef_matrix(i,:) = coef_matrix(CX,:); 
   end 
 end
  
        %check if input is integer 
       	int_sti=round(stimulation); 
	    T = (int_sti ~= stimulation); 
	    if (max(T)~=0) 
	        fprintf('WARNING : Integer Input Expected: Rounding Fractional Input to Nearest Integer...\n'); 
	    end 
	     
	    %Input overflow check 
	    switch  data_type 
	    case 1 
	        %set max/min for signed 
	        maxdat = 2^(DATA_WIDTH-1)-1; 
	        mindat = -maxdat-1; 
	    case 2 
	        %set max/min for unsigned 
	        maxdat = 2^DATA_WIDTH-1; 
	        mindat = 0; 
	    end 

	    if(data_type == 2)
	    	if(abs(coef_matrix) == coef_matrix)
	    		FIR_WIDTH = FIR_WIDTH +1;
	    	end
	    end

	    %Saturating Input Value 
	    a=find(int_sti>maxdat); 
	    b=find(int_sti<mindat); 
	    if (~isempty(a)|~isempty(b)) 
	 	    fprintf('WARNING : Input Amplitude Exceeds MAXIMUM/MINIMUM allowable values - saturating input values...\n'); 
	            lena = length (a); 
	            lenb = length (b); 
	            for i =1:lena 
	        	    fprintf('%d > %d \n', int_sti(a(i)), maxdat); 
			        int_sti(a(i)) = maxdat; 
		        end 
		    for i =1:lenb 
			    fprintf('%d < %d \n', int_sti(b(i)), mindat); 
			    int_sti(b(i)) = mindat; 
		    end 
	    end 
         
	    % Add interpolation 
   	    inter_sti = zeros(1, INTER_FACTOR * length(int_sti)); 
	    inter_sti(1:INTER_FACTOR:INTER_FACTOR * length(int_sti)) = int_sti; 
 
         
        for i = 1 : DY *INTER_FACTOR 
    	    coef_current = coef_matrix(i,:); 
            output_temp(i) = simp_adaptive (inter_sti, coef_current, i); 
        end 
	% Truncate output 
	len1 = length(output_temp); 
	 
	    switch  LSB_TYPE 
	    case 0 
	        %truncate 
            out_dec = bi_trunc_lsb(output_temp,LSB_RM,FIR_WIDTH); 
	    case 1 
	        %round 
            out_dec = bi_round(output_temp,LSB_RM, FIR_WIDTH); 
	    end 
         
 	    switch  MSB_TYPE 
	    case 0 
	        %truncate 
            out_dec = bi_trunc_msb(out_dec,MSB_RM,FIR_WIDTH-LSB_RM); 
	    case 1 
	        %round 
            out_dec = bi_satu(out_dec,MSB_RM, FIR_WIDTH-LSB_RM); 
	    end 
 	    
    	% choose decimation output in phase=DECI_FACTOR-1  
     	if(DECI_FACTOR == 1) 
     		output = out_dec; 
     	else
     		output = out_dec;%(DECI_FACTOR:DECI_FACTOR:len1);
 	end  
 	       
  	function[output, outindex] = simp_adaptive (int_sti, coef_current, data_index, output) 
	%Simulation is the whole input sequence 
	%coef_current is the current coefficient set 
	%data_index gives the last data to use 
	%outputs are the sum of input and coef multiplication 
	%outindex is the next data_index 
    
	sti_current = zeros(length(coef_current),1); 
	 
	data_length = length(int_sti); 
	 
	%Check data index 
	if (data_index > data_length) 
		fprintf('ERROR: DATA INDEX IS LARGER THAN DATA LENGTH!!!\n'); 
		return; 
	end 
	for i = 1: length(coef_current) 
	   if ((data_index -i+1)>0 & (data_index - i+1)<=data_length) 
	      sti_current(i,1) = int_sti(data_index - i+1); 
	   end 
	end 
	 
	outindex= data_index+1; 
	output = coef_current * sti_current; 
	% end of function simp_adaptive 
 
	function output = bi_round(data_in,LSB_RM,ORI_WIDTH, output) 
	% LSB_RM is the bit to lose in LSB 
	% ORI_WIDTH is the original data width
	data = round (data_in / 2^LSB_RM);       
	output = bi_satu(data,0,ORI_WIDTH - LSB_RM); 
	%end of function bi_trunc_lsb 
	 
	function output = bi_trunc_lsb(data_in,LSB_RM,ORI_WIDTH, output) 
	% LSB_RM is the bit to lose in LSB 
	% ORI_WIDTH is the original data width 
	%2's complement system 
	output = bitshift(2^ORI_WIDTH*(data_in<0) + data_in, -LSB_RM) - 2^(ORI_WIDTH-LSB_RM) *(data_in<0); 
	% end of function bi_round 
	 
	function output = bi_trunc_msb(data_in,MSB_RM,ORI_WIDTH, output) 
	% MSB_RM is the bit to lose in LSB 
	% ORI_WIDTH is the original data width 
	%2's complement system 
	data = 2^ORI_WIDTH * (data_in < 0)+ data_in; 
	erase_num = 2^(ORI_WIDTH - MSB_RM) - 1; 
	data = bitand(data, erase_num); 
	output = data - 2^(ORI_WIDTH - MSB_RM)*(bitget(data,ORI_WIDTH - MSB_RM)); 
	%end of bi_trunc_msb 
	 
	function output = bi_satu(data_in,MSB_RM,ORI_WIDTH, output) 
	% MSB_RM is the bit to lose in LSB 
	% ORI_WIDTH is the original data width 
	%2's complement system 
	maxdat = 2^(ORI_WIDTH - MSB_RM - 1)-1; 
	mindat = 2^(ORI_WIDTH - MSB_RM - 1)*(-1); 
	data_in(find(data_in > maxdat)) = maxdat; 
	data_in(find(data_in < mindat)) = mindat; 
	output = data_in; 
	%end of bi_satu  
         
