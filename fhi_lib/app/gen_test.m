%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
% Copyright (c) 2021 Intel.
% 
% Licensed under the Apache License, Version 2.0 (the "License");
% you may not use this file except in compliance with the License.
% You may obtain a copy of the License at
% 
% http://www.apache.org/licenses/LICENSE-2.0
% 
% Unless required by applicable law or agreed to in writing, software
% distributed under the License is distributed on an "AS IS" BASIS,
% WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
% See the License for the specific language governing permissions and
% limitations under the License.
% 
%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%This script was tested with GNU Octave, version 3.8.2 or Matlab 9.2.0.538062 (R2017a) 

close all;
clear all;

     %  5MHz    10MHz   15MHz   20 MHz  
nLteNumRbsPerSymF1 = ...
[
     %  5MHz    10MHz   15MHz   20 MHz  
        [25,    50,     75,     100]         % Numerology 0 (15KHz)
];

     %  5MHz    10MHz   15MHz   20 MHz  25 MHz  30 MHz  40 MHz  50MHz   60 MHz  70 MHz  80 MHz   90 MHz  100 MHz
nNumRbsPerSymF1 = ...
[
     %  5MHz    10MHz   15MHz   20 MHz  25 MHz  30 MHz  40 MHz  50MHz   60 MHz  70 MHz  80 MHz   90 MHz  100 MHz
        [25,    52,     79,     106,    133,    160,    216,    270,    0,         0,      0,      0,      0]         % Numerology 0 (15KHz)
        [11,    24,     38,     51,     65,     78,     106,    133,    162,       0,    217,    245,    273]         % Numerology 1 (30KHz)
        [0,     11,     18,     24,     31,     38,     51,     65,     79,        0,    107,    121,    135]         % Numerology 2 (60KHz)
];

nNumRbsPerSymF2 = ...
[
    %  50Mhz  100MHz  200MHz   400MHz
        [66,    132,    264,     0]        % Numerology 2 (60KHz)
        [32,    66,     132,     264]      % Numerology 3 (120KHz)
];

% total number of tests
tests_total = 12
tech_all = ... % 0 - NR 1- LTE
    [ 
      0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1  
    ]

sub6_all = ...
    [ 
      true, true, true, true, false, true, true, true, true, true, true, true
    ]

mu_all = ...
    [
      0, 0, 0, 1, 3, 1, 0, 0, 0, 0, 0, 0
    ]

bw_all = ...
    [
      5, 10, 20, 100, 100, 100, 20, 10, 5, 20, 10, 5
    ]

ant_num_all = ...
    [
      4, 4, 4, 4, 4, 8, 4, 4, 4, 8, 8, 8
    ]

bfw_gen_all = ...
    [
      false, false, false, false, false, true, false, false, false, true, true, true,
    ]

trx_all = ...
    [ 
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32
    ]
path_to_usecase_all = ... 
    [
      "./usecase/cat_a/mu0_5mhz/";
      "./usecase/cat_a/mu0_10mhz/";
      "./usecase/cat_a/mu0_20mhz/";
      "./usecase/cat_a/mu1_100mhz/";
      "./usecase/cat_a/mu3_100mhz/";
      "./usecase/cat_b/mu1_100mhz/";
      "./usecase/lte_a/mu0_20mhz/";
      "./usecase/lte_a/mu0_10mhz/";
      "./usecase/lte_a/mu0_5mhz/";
      "./usecase/lte_b/mu0_20mhz/";
      "./usecase/lte_b/mu0_10mhz/";
      "./usecase/lte_b/mu0_5mhz/";      
    ]

path_to_usecase_all = cellstr(path_to_usecase_all) 

nSlots_all = ...
    [
       20,20,20,20,20,20,20,20,20,10,10,10
    ]

%select mu and bw to generate test files
for test_num =(1:1:tests_total)
    test_num
    isLte=tech_all(test_num)  %false 
    sub6=sub6_all(test_num)  %false
    mu=mu_all(test_num) % 0,1, or 3
    bw=bw_all(test_num) %5,10,20,100 MHz
    ant_num = ant_num_all(test_num)
    bfw_gen=bfw_gen_all(test_num)
    trx = trx_all(test_num)
    size(path_to_usecase_all)
    path_to_usecase = path_to_usecase_all(test_num)

    nSlots=nSlots_all(test_num) % any 40 and 160

    if isLte
        disp('LTE')
        if mu < 3
            nNumerology = mu+1;
            switch (bw)
                case {5}
                    numRBs = nLteNumRbsPerSymF1(nNumerology,0+1);
                case {10}
                    numRBs = nLteNumRbsPerSymF1(nNumerology,1+1);
                case {15}
                    numRBs = nLteNumRbsPerSymF1(nNumerology,2+1);
                case {20}
                    numRBs = nLteNumRbsPerSymF1(nNumerology,3+1);
            end
        end
    else
        if sub6
            disp('Sub6')
            if mu < 3
                nNumerology = mu+1;
                switch (bw)
                    case {5}
                        numRBs = nNumRbsPerSymF1(nNumerology,0+1);
                    case {10}
                        numRBs = nNumRbsPerSymF1(nNumerology,1+1);
                    case {15}
                        numRBs = nNumRbsPerSymF1(nNumerology,2+1);
                    case {20}
                        numRBs = nNumRbsPerSymF1(nNumerology,3+1);
                    case {25}
                        numRBs = nNumRbsPerSymF1(nNumerology,4+1);
                    case {30}
                        numRBs = nNumRbsPerSymF1(nNumerology,5+1);
                    case {40}
                        numRBs = nNumRbsPerSymF1(nNumerology,6+1);
                    case {50}
                        numRBs = nNumRbsPerSymF1(nNumerology,7+1);
                    case {60}
                        numRBs = nNumRbsPerSymF1(nNumerology,8+1);
                    case {70}
                        numRBs = nNumRbsPerSymF1(nNumerology,9+1);
                    case {80}
                        numRBs = nNumRbsPerSymF1(nNumerology,10+1);
                    case {90}
                        numRBs = nNumRbsPerSymF1(nNumerology,11+1);
                    case {100}
                        numRBs = nNumRbsPerSymF1(nNumerology,12+1);
                    otherwise
                        disp('Unknown BW && mu')
                end
            end
        else
            disp('mmWave')
            if  (mu >=2) && (mu <= 3)
                nNumerology = mu;
                switch (bw)
                    case {50}
                        numRBs = nNumRbsPerSymF2(nNumerology-1,0+1);
                    case {100}
                        numRBs = nNumRbsPerSymF2(nNumerology-1,1+1);
                    case {200}
                        numRBs = nNumRbsPerSymF2(nNumerology-1,2+1);
                    case {400}
                        numRBs = nNumRbsPerSymF2(nNumerology-1,3+1);
                    otherwise
                        disp('Unknown BW && mu')
                end
            end
        end
    end
    if numRBs ==0
        disp('Incorrect Numerology and BW combination.')
        return
    end

    bw
    numRBs
    nSlots

    %use file as input
    %ifft_in = load('ifft_in.txt')

    %gen IQs
    ifft_in = [[1:1:(numRBs*12)]', [1:1:(numRBs*12)]'];

    ant_c = ifft_in;
    for (i=1:1:nSlots*14-1)
        ifft_in_1 = ifft_in + i;
        ant_c = [ant_c; ifft_in_1];
    end

    %write files for IQ samples 
    for ant = 1:1:ant_num
        antX=ant_c*(ant*10);
        antX_16=int16(antX.');
        file_name = strcat(path_to_usecase,"ant_", num2str(ant-1),".bin");
        disp(file_name)
        fileID = fopen(file_name,'w');
        fwrite(fileID, antX_16, 'int16');
        fclose(fileID);    
    end

    if bfw_gen
        disp('Generate BF Weights per RB')

        %seed to make it repeatable 
        rand('seed',47)

        %random channel matrix for single sym on syngle RB 
        H = (rand(trx,ant_num) + 1j*rand(trx,ant_num));

        %calculate weights 
        % W_dl = H^*(H^TH^*)^-1
        % W_ul =  ((H^H*H)^-1)H^H
        % where H^* - conjugate 
        %       H^T - transpose
        %       H^H - conjugate transpose 
        W_dl = conj(H)*(transpose(H)*conj(H))^-1; %weights for DL
        W_ul = ((ctranspose(H)*H)^-1)*ctranspose(H); %weights for UL

        W_ul = W_ul.';

        for ant = 1:1:ant_num
            %DL 

            bfw_per_sym = [];
            % adjust channel per each RB 
            for iPrb = 1:1:numRBs
               bfw_per_sym = [ bfw_per_sym, [real((W_dl(:, ant).'))*iPrb; imag((W_dl(:, ant).'))*iPrb]];
            end

            bfw_all_slots = [];
            %reuse channel for all symbols  
            for (slot_idx=1:1:nSlots*14)
                bfw_all_slots = [bfw_all_slots, bfw_per_sym];
            end

            bfw_all_slots_int = int16(bfw_all_slots./max(max(abs((bfw_all_slots.')))).*2^15);

            %write files for IQ samples 
            antX_16=bfw_all_slots_int.';
            file_name = strcat(path_to_usecase,"dl_bfw_ue_", num2str(ant-1),".bin");
            disp(file_name)
            fileID = fopen(file_name,'w');
            fwrite(fileID,antX_16, 'int16');
            fclose(fileID);    

            %UL 
            bfw_per_sym = [];
            % adjust channel per each RB 
            for iPrb = 1:1:numRBs
               bfw_per_sym = [ bfw_per_sym, [real((W_ul(:, ant).'))*iPrb; imag((W_ul(:, ant).'))*iPrb]];
            end

            bfw_all_slots = [];
            %reuse channel for all symbols  
            for (slot_idx=1:1:nSlots*14)
                bfw_all_slots = [bfw_all_slots, bfw_per_sym];
            end

            bfw_all_slots_int = int16(bfw_all_slots./max(max(abs((bfw_all_slots.')))).*2^15);

            %write files for IQ samples 
            antX_16=bfw_all_slots_int.';
            file_name = strcat(path_to_usecase,"ul_bfw_ue_", num2str(ant-1),".bin");
            disp(file_name)
            fileID = fopen(file_name,'w');
            fwrite(fileID,antX_16, 'int16');
            fclose(fileID);    
        end  
    end
end

%% generate IQ file with valid constellation, for DL modulation compression
% only in mu1_100mhz
%constellation = [4096, -4096];
%constellation = [2590, 7770, -7770, -2590];
%constellation = [633, 1897, 3161, 4425, -4424, -3160, -1897, -633];
constellation_all = [628, 1885, 3141, 4398, 5654, 6911, 8167, 9424, -9424, -8167, -6911, -5654, -4398, -3141, -1885, -628;
    633, 1897, 3161, 4425, -4424, -3160, -1897, -633, 633, 1897, 3161, 4425, -4424, -3160, -1897, -633;
    2590, 7770, -7770, -2590, 2590, 7770, -7770, -2590, 2590, 7770, -7770, -2590, 2590, 7770, -7770, -2590;
    4096, -4096, 4096, -4096, 4096, -4096, 4096, -4096, 4096, -4096, 4096, -4096, 4096, -4096, 4096, -4096;
    ];

numRBs = 273
nSlots = 20
path_all = ...
    [
      "./usecase/cat_a/mu1_100mhz/";
      "./usecase/cat_b/mu1_100mhz/";
      "./usecase/cat_b/mu1_100mhz/";
      "./usecase/cat_b/mu1_100mhz/";
    ]
path_all = cellstr(path_all)
modtype_all = ...
    [
      "256qam_ant_";
      "64qam_ant_";
      "16qam_ant_";
      "qpsk_ant_";
    ]
modtype_all = cellstr(modtype_all)

for test_num = 1:4
    path = path_all(test_num);
    constellation=constellation_all(test_num,:);
    modtype = modtype_all(test_num);
    for ant = 1:4
        ant_in = rand(2*12*numRBs*14*nSlots,1); % random constellation
        ant_in = 1+round(15 * ant_in);
        ant_out = constellation(ant_in);
        file_name = strcat(path,modtype, num2str(ant-1),".bin");
        disp(file_name)
        fileID = fopen(file_name,'w');
        fwrite(fileID, ant_out, 'int16');
        fclose(fileID);
    end
end


