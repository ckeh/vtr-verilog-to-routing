tic

%initialize tester class
t = Tester();

t.architecture = 'k6_frac_N10_mem32K_40nm_mine.xml';
t.archPath = [t.vtrPath '/vtr_flow/arch/timing/' t.architecture];

%variable setup
benchmarks_dir = [t.vtrPath '/vtr_flow/benchmarks/vtr_benchmarks_blif/'];
%'LU32PEEng',...                 %huge circuit. also slow
%'stereovision2',...		 %also a huge circuit. takes ~24 hours	
benchmark_list = {'bgm',...
'blob_merge',...
'boundtop',...
'ch_intrinsics',...
'diffeq1',...
'diffeq2',...
'LU8PEEng',...
'mcml',...
'mkDelayWorker32B',...
'mkPktMerge',...
'mkSMAdapter4B',...
'or1200',...
'raygentop',...
'sha',...
'stereovision0',...
'stereovision1',...
'stereovision3'};
benchmark_list_path = strcat(benchmark_list, '.blif');
benchmark_list_path = strcat(benchmarks_dir, benchmark_list_path);
numCkts = length(benchmark_list_path);
arch = t.archPath;

vprBaseOptions = '-nodisp';
vprOptionsFullFlow = vprBaseOptions;

labels = { 
            'Low Stress Delay',...
            'Chan Width',...
            'Low Stress Wirelength',....
            'Per-tile LB Routing Area',...
            'LS clb PD',...
            'LS clb WH',...
            'LS clb HD',...
            'LS clb HP',...
            'LS clb PH'
          };

%parseRegex ordering has to match labels ordering
parseRegex = {
                'Final critical path: (\d*\.*\d*)',...
                'channel width factor of (\d+)',... 
                'Total wirelength: (\d+)',...
                '\s*Assuming no buffer sharing \(pessimistic\)\. Total: \d+\.\d+e\+\d+, per logic tile: (\d+)',...
                'clb\s+Pin Diversity:\s+(\d*\.*\d*)',...
                'clb\s+Pin Diversity:\s+\d*\.*\d*\s+Wire Homogeneity:\s+(\d*\.*\d*)',...
                'clb\s+Pin Diversity:\s+\d*\.*\d*\s+Wire Homogeneity:\s+\d*\.*\d*\s+Hamming Distance:\s+(\d*\.*\d*)',...
                'clb\s+Pin Diversity:\s+\d*\.*\d*\s+Wire Homogeneity:\s+\d*\.*\d*\s+Hamming Distance:\s+\d*\.*\d*\s+Hamming Proximity:\s+(\d*\.*\d*)',...
                'clb\s+Pin Diversity:\s+\d*\.*\d*\s+Wire Homogeneity:\s+\d*\.*\d*\s+Hamming Distance:\s+\d*\.*\d*\s+Hamming Proximity:\s+\d*\.*\d*\s+Pin Homogeneity:\s+(\d*\.*\d*)'
             };

         
% run from Fc_out = 0.1 to Fc_out = 0.3 in steps of 0.1         
metricRange = 0.10:0.10:0.30;

%if we use 4 workers, we can run 2 jobs in parallel; and it probably wont affect speed that much cause one of the big circuits
% is the real bottleneck the other 3 workers will eventually have to wait for 
matlabpool open 4;

% make VPR
t.replaceSingleLineInFile('/*#define TEST_METRICS', '//#define TEST_METRICS', t.rrGraphPath);
t.makeVPR();

i = 0;
for metric = metricRange
    i = i + 1;  
    
    result = t.replaceFcInArchFile(arch, 0.15, metric);
    
    lowStressW = 0;
    %get low stress channel widths then get delay at low stress W
    parfor ickt = 1:numCkts
        benchmark = benchmark_list_path{ickt};

        pause(ickt);
        vprString = [arch ' ' benchmark ' ' vprBaseOptions];
        vprOut = t.runVprManual(vprString);

        %get min chan width
        minW = t.regexLastToken(vprOut, '.*channel width factor of (\d+).');
        minW = str2double(minW);
        lowStressW(ickt) = floor(minW*1.3);

        display(['ckt: ' benchmark_list{ickt} ' hi-stress W: ' num2str(minW) '  low-stress W: ' num2str(lowStressW(ickt))]);
        
        %now get delay at the low stress W
        vprString = [arch ' ' benchmark ' ' vprBaseOptions ' -route_chan_width ' num2str(lowStressW(ickt))];
        vprOut = t.runVprManual(vprString);
	temp_parfor_var = 0;
        for imetric = 1:length(parseRegex)
	   temp_parfor_var(imetric) = str2double(t.regexLastToken(vprOut, parseRegex{imetric}));
        end
	adjustedCktMetrics(ickt, :) = temp_parfor_var;
    end
    
    %now have to compute the geometric average
    adjustedAvgMetrics(i,:) = geomean(adjustedCktMetrics,1);
end

%matlabpool('close');

%print data to file
%baselineAvgMetrics = [metricRange' baselineAvgMetrics];
labels = ['metric' labels];
%and then print
%t.printDataToFile('./run_metrics.txt', baselineAvgMetrics, labels, false);

adjustedAvgMetrics = [metricRange' adjustedAvgMetrics];
append = false;
t.printDataToFile('./run_metrics_Wilton++.txt', adjustedAvgMetrics, labels, append);

toc

exit;