#include "circuit.h"

string logicRam::get_lram_mode() {
    string mode_result;
    switch (mode){
        case SimpleDualPort:
            mode_result = "SimpleDualPort";
        break;
        case ROM:
            mode_result = "ROM";
        break;
        case SinglePort:
            mode_result = "SinglePort";
        break;
        case TrueDualPort:
            mode_result = "TrueDualPort";
        break;
    }
    return mode_result;
}

void resource::prepare_combination(){
    for (int poss_width = max_width; poss_width >= 1; poss_width /= 2){
        unsigned int poss_depth = size/poss_width;
        pair<unsigned int, unsigned int> combination(poss_depth, poss_width);
        add_dw_combination(combination);
    }
}


void construct_resource(vector<resource>& resource_list, operationType op, input_parameter input_pack) {
    if(op == STRATIX_IV){
        //insert LUTRAM resource
        vector<pair<unsigned int, unsigned int> > comb;
        resource lut_resource = resource(LUTRAM, 2.0, 64, 20, 640, comb);
        //directly insert, since only two combination available
        pair<unsigned int, unsigned int> comb1(64, 10);
        pair<unsigned int, unsigned int> comb2(32, 20);
        lut_resource.add_dw_combination(comb1);
        lut_resource.add_dw_combination(comb2);
        resource_list.push_back(lut_resource);

        //insert 8192 BRAM
        comb.empty();
        resource bram8192_resource = resource(BRAM_8192, 10.0, 8192, 32, 8192, comb);
        bram8192_resource.prepare_combination();
        resource_list.push_back(bram8192_resource);

        //insert 128k BRAM
        comb.empty();
        resource bram128k_resource = resource(BRAM_128K, 300.0, 131072, 128, 131072, comb);
        bram128k_resource.prepare_combination();
        resource_list.push_back(bram128k_resource);
    }else if(op == NO_LUTRAM || op == WITH_LUTRAM){
        //insert the only type of BRAM according to the input
        vector<pair<unsigned int, unsigned int> > comb;
        unsigned int ratio = input_pack.get_input_bram_ratio()[0];
        unsigned int bram = input_pack.get_input_bram_size()[0];
        unsigned int m_width = input_pack.get_input_bram_mwidth()[0];
        resource bramcustom_resource = resource(BRAM_CUSTOM, ratio, bram, m_width, bram, comb);
        bramcustom_resource.prepare_combination();
        resource_list.push_back(bramcustom_resource);
        if(op == WITH_LUTRAM){
            //insert LUTRAM resource
            vector<pair<unsigned int, unsigned int> > comb;
            resource lut_resource = resource(LUTRAM, 2.0, 64, 20, 640, comb);
            //directly insert, since only two combination available
            pair<unsigned int, unsigned int> comb1(64, 10);
            pair<unsigned int, unsigned int> comb2(32, 20);
            lut_resource.add_dw_combination(comb1);
            lut_resource.add_dw_combination(comb2);
            resource_list.push_back(lut_resource);
        }
    }else if(op == MB_WITH_LUTRAM){
        //insert LUTRAM resource
        vector<pair<unsigned int, unsigned int> > comb;
        resource lut_resource = resource(LUTRAM,input_pack.get_input_lutram_ratio(), 64, 20, 640, comb);
        //directly insert, since only two combination available
        pair<unsigned int, unsigned int> comb1(64, 10);
        pair<unsigned int, unsigned int> comb2(32, 20);
        lut_resource.add_dw_combination(comb1);
        lut_resource.add_dw_combination(comb2);
        resource_list.push_back(lut_resource);

        comb.empty();
        resource bramcustom_resource = resource(BRAM_CUSTOM, input_pack.get_input_bram_ratio()[0],
                input_pack.get_input_bram_size()[0],input_pack.get_input_bram_mwidth()[0],
                input_pack.get_input_bram_size()[0], comb);
        bramcustom_resource.prepare_combination();
        resource_list.push_back(bramcustom_resource);

        comb.empty();
        resource bramcustom2_resource = resource(BRAM_CUSTOM_2, input_pack.get_input_bram_ratio()[1],
                input_pack.get_input_bram_size()[1],input_pack.get_input_bram_mwidth()[1],
                input_pack.get_input_bram_size()[1], comb);
        bramcustom2_resource.prepare_combination();
        resource_list.push_back(bramcustom2_resource);
    }
}

void perform_basic_core_mapper(vector<circuit>& logic_circuit_list, vector<resource>& resource_list){
    for (auto& circuit: logic_circuit_list){
        unsigned int existing_LB = circuit.get_circuit_num_lb();
        unsigned int used_lutram = 0;
        unsigned int used_8192bram = 0;
        unsigned int used_128kbram = 0;
        int id_count = 0;
//        circuit.set_circuit_area(existing_LB * (35000+40000)/2); //MAGIC NUMBER HERE
        double circuit_areatested = 0.0;
//        cout<<"process circuit id: "<<circuit.get_circuit_id()<<endl;
//        cout<<"it has: "<<circuit.get_ram_list().size()<<" number of logic rams"<<endl;

        int logicram_count = 0;
        for(auto& logicram: circuit.get_ram_list()){
            //current cheapest mapped RAM for this current logic ram across all possible physical candidates
            vector<mappedRam> cheapest_map_list;
            double cheapest_area = DBL_MAX;

            for(auto physical_candidate: resource_list){
                if(physical_candidate.get_pram_type() == LUTRAM && logicram.get_lram_mode() == "TrueDualPort"){
                    // lutram cannot support TDP
                    continue;
                }

                //iterate through all possible dw combinations of current physical candidate
                for(auto dw_pair: physical_candidate.get_comb_list()){
                    unsigned int curr_depth = dw_pair.first;
                    unsigned curr_width = dw_pair.second;

                    if(curr_width == physical_candidate.get_max_width() && logicram.get_lram_mode() == "TrueDualPort"){
                        //widest width is not available for TDP
                        continue;
                    }

                    int mapper_id = id_count ++;
                    //decide how to locate logic using physical candidate
                    unsigned int p = 1;
                    unsigned int s = 1;
                    unsigned int num_luts = 0;
                    if(logicram.get_lram_width() > curr_width){
                        p = (unsigned int)ceil((double)logicram.get_lram_width()/(double)curr_width);
                    }
                    if(logicram.get_lram_depth() > curr_depth){
                        s = (unsigned int)ceil((double)logicram.get_lram_depth()/(double)curr_depth);
                        if (s > 16){
                            //dont consider any solution that is 16x deeper
                            continue;
                        }
                        //compute extra logic needed(num_of_luts) when in serial
                        if(s <= 4){
                            if(s == 2){
                                num_luts = 1*logicram.get_lram_width() + 1;
                            }else{
                                num_luts = 1*logicram.get_lram_width() + s;
                            }
                        }else if(s <= 7){
                            num_luts = 2*logicram.get_lram_width() + s;
                        }else if(s <= 10){
                            num_luts = 3*logicram.get_lram_width() + s;
                        }else if(s <= 13){
                            num_luts = 4*logicram.get_lram_width() + s;
                        }else{
                            num_luts = 5*logicram.get_lram_width() + s;
                        }
                    }
                    if(logicram.get_lram_mode() == "TrueDualPort") {// NOT SURE
                        num_luts *= 2;
                    }
                    //test the area if use this pram, this combination
                    unsigned int num_pram_plan = s * p;
                    unsigned int extra_logic_LB = ceil((double)num_luts / 10.0);//MAGIC NUMBER HERE
                    unsigned int LB_plan = 0; // total LB if use this pram
                    unsigned int bram8192_plan = 0;
                    unsigned int bram128k_plan = 0;
                    if(physical_candidate.get_pram_type() == LUTRAM){
                        LB_plan = existing_LB + used_lutram + num_pram_plan + extra_logic_LB;
                    }else{
                        LB_plan = existing_LB + used_lutram + extra_logic_LB;
                        if(physical_candidate.get_pram_type() == BRAM_8192) {
                            bram8192_plan = used_8192bram + num_pram_plan;
                        }else {
                            bram128k_plan = used_128kbram + num_pram_plan;
                        }
                    }

                    unsigned int LBrequired_plan;
                    if(LB_plan >= bram8192_plan * 10 && LB_plan >= bram128k_plan * 300){
                        LBrequired_plan = LB_plan;
                    }else if(bram8192_plan * 10 >= LB_plan && bram8192_plan * 10 >= bram128k_plan * 300){
                        LBrequired_plan = bram8192_plan * 10;
                    }else{
                        LBrequired_plan = bram128k_plan * 300;
                    }
                    int required_8192 = floor((double)LBrequired_plan/(double)10);
                    int required_128k = floor((double)LBrequired_plan/(double)300);
                    double try_area = LBrequired_plan * 37500 + required_8192 * (9000 + 5 * 8192 + 90 * sqrt((double)8192) + 600 * 2 * 32)
                            + required_128k * (9000 + 5 * 131072 + 90 * sqrt((double)131072) + 600 * 2 * 128);
                    if(try_area < cheapest_area){
                        if(try_area == 0){
                            cout<<"Something went wrong, area should not be zero!!!"<<endl;
                            exit(1);
                        }
                        cheapest_area = try_area;
                        cheapest_map_list.insert(cheapest_map_list.begin(),
                                mappedRam(logicram.get_lram_id(), mapper_id, num_luts, logicram.get_lram_depth(),
                                        logicram.get_lram_width(), s, p, physical_candidate.get_pram_type(),
                                        logicram.get_lram_mode(), curr_depth, curr_width, try_area));
                    }
                }// iterate through each combination of the same physical ram
            }// iterate through all physical candidates, we have found the cheapset map for this logic ram

            if(cheapest_map_list.empty()){
                cout<<"No available mapped result found!!!"<<endl;
                cout<<"Something went wrong"<<endl;
                exit(1);
            }
            mappedRam cheapest_map = cheapest_map_list[0];
            logic_circuit_list[circuit.get_circuit_id()].add_mapped_ram(cheapest_map);

            existing_LB += ceil((double)(cheapest_map.get_lut()) / 10.0);//MAGIC NUMBER HERE
            if(cheapest_map.get_map_type() == LUTRAM){
                used_lutram += cheapest_map.get_s() * cheapest_map.get_p();
            }else if(cheapest_map.get_map_type() == BRAM_8192){
                used_8192bram += cheapest_map.get_s() * cheapest_map.get_p();
            }else{
                used_128kbram += cheapest_map.get_s() * cheapest_map.get_p();
            }
            logicram_count++;
            if(logicram_count == circuit.get_ram_list().size()){
                circuit_areatested = cheapest_map.get_total_cost();
                if(circuit_areatested == 0){
                    cout<<"area got from mapped is zero!!!"<<endl;
                    cout<<"Something went wrong"<<endl;
                    exit(1);
                }
                //print for debug purpose: this current circuit info
                cout<<"circuit: "<<circuit.get_circuit_id()<<" used LUTRAM: "<<used_lutram
                <<", used 8192BRAM: "<<used_8192bram
                <<", used 128k BRAM: "<<used_128kbram
                <<", my tested area is: "<<circuit_areatested<<endl;
            }
        }// all logic ram in this circuit have been mapped
        logic_circuit_list[circuit.get_circuit_id()].set_circuit_area(circuit_areatested);
    }
}

void perform_custom_core_mapper(vector<circuit>& logic_circuit_list, vector<resource>& resource_list, operationType op,
                                vector<unsigned int>ratio_list){
    for (auto& circuit: logic_circuit_list){
        unsigned int existing_LB = circuit.get_circuit_num_lb();
        unsigned int used_lutram = 0;
        unsigned int used_cusbram = 0;
        unsigned int used_cusbram2 = 0;
        int id_count = 0;
        double circuit_areatested = 0.0;

        int logicram_count = 0;
        for(auto& logicram: circuit.get_ram_list()){
            //current cheapest mapped RAM for this current logic ram across all possible physical candidates
//            cout<<"processing circuit: "<<circuit.get_circuit_id()<<", logic ram id: "<<logicram_count<<endl;
            vector<mappedRam> cheapest_map_list;
            double cheapest_area = DBL_MAX;

            for(auto physical_candidate: resource_list){
                if(physical_candidate.get_pram_type() == LUTRAM && logicram.get_lram_mode() == "TrueDualPort"){
                    // lutram cannot support TDP
                    continue;
                }

                //iterate through all possible dw combinations of current physical candidate
                for(auto dw_pair: physical_candidate.get_comb_list()){
                    unsigned int curr_depth = dw_pair.first;
                    unsigned curr_width = dw_pair.second;

                    if(curr_width == physical_candidate.get_max_width() && logicram.get_lram_mode() == "TrueDualPort"){
                        //widest width is not available for TDP
                        continue;
                    }

                    int mapper_id = id_count ++;
                    //decide how to locate logic using physical candidate
                    unsigned int p = 1;
                    unsigned int s = 1;
                    unsigned int num_luts = 0;
                    if(logicram.get_lram_width() > curr_width){
                        p = (unsigned int)ceil((double)logicram.get_lram_width()/(double)curr_width);
                    }
                    if(logicram.get_lram_depth() > curr_depth){
                        s = (unsigned int)ceil((double)logicram.get_lram_depth()/(double)curr_depth);
                        if (s > 16){
                            //dont consider any solution that is 16x deeper
                            continue;
                        }
                        //compute extra logic needed(num_of_luts) when in serial
                        if(s <= 4){
                            if(s == 2){
                                num_luts = 1*logicram.get_lram_width() + 1;
                            }else{
                                num_luts = 1*logicram.get_lram_width() + s;
                            }
                        }else if(s <= 7){
                            num_luts = 2*logicram.get_lram_width() + s;
                        }else if(s <= 10){
                            num_luts = 3*logicram.get_lram_width() + s;
                        }else if(s <= 13){
                            num_luts = 4*logicram.get_lram_width() + s;
                        }else{
                            num_luts = 5*logicram.get_lram_width() + s;
                        }
                    }
                    if(logicram.get_lram_mode() == "TrueDualPort") {// NOT SURE
                        num_luts *= 2;
                    }
                    //test the area if use this pram, this combination
                    unsigned int num_pram_plan = s * p;
                    unsigned int extra_logic_LB = ceil((double)num_luts / 10.0);//MAGIC NUMBER HERE
                    unsigned int LB_plan = 0; // total LB if use this pram
                    unsigned int bram_plan = 0;
                    unsigned int bram1_plan = 0;
                    unsigned int bram2_plan = 0;
                    if(physical_candidate.get_pram_type() == LUTRAM){
                        LB_plan = existing_LB + used_lutram + num_pram_plan + extra_logic_LB;
                    }else{
                        LB_plan = existing_LB + used_lutram + extra_logic_LB;
                        if(op == MB_WITH_LUTRAM){
                            if(physical_candidate.get_pram_type() == BRAM_CUSTOM){
                                bram1_plan = used_cusbram + num_pram_plan;
                            }else{
                                bram2_plan = used_cusbram2 + num_pram_plan;
                            }
                        }else{
                            bram_plan = used_cusbram + num_pram_plan;
                        }
                    }

                    unsigned int LBrequired_plan;
                    if(op == MB_WITH_LUTRAM){
                        if(LB_plan >= bram1_plan * ratio_list[1] && LB_plan >= bram2_plan * ratio_list[2]){
                            LBrequired_plan = LB_plan;
                        }else if(bram1_plan * ratio_list[1] >= LB_plan && bram1_plan * ratio_list[1]
                        >= bram2_plan * ratio_list[2]){
                            LBrequired_plan = bram1_plan * ratio_list[1];
                        }else{
                            LBrequired_plan = bram2_plan * ratio_list[2];
                        }
                    }else{
                        if(LB_plan >= bram_plan * physical_candidate.get_ratio()){
                            LBrequired_plan = LB_plan;
                        }else{
                            LBrequired_plan = bram_plan * physical_candidate.get_ratio();
                        }
                    }

                    int required_bram = floor((double)LBrequired_plan/(double)physical_candidate.get_ratio());
                    double try_area;
                    unsigned int bram_bits = physical_candidate.get_pram_size();
                    unsigned int bram_mwidth = physical_candidate.get_max_width();
                    if(op == NO_LUTRAM){
                        try_area = LBrequired_plan * 35000 + required_bram * (9000 +
                                5 * bram_bits + 90 * sqrt((double)bram_bits) + 600 * 2 * bram_mwidth);
                    }else if(op == WITH_LUTRAM){
                        try_area = LBrequired_plan * 37500 + required_bram * (9000 +
                                5 * bram_bits + 90 * sqrt((double)bram_bits) + 600 * 2 * bram_mwidth);
                    }else{
                        unsigned int bram1_bits = resource_list[1].get_pram_size();
                        unsigned int bram1_mwidth = resource_list[1].get_max_width();
                        unsigned int bram2_bits = resource_list[2].get_pram_size();
                        unsigned int bram2_mwidth = resource_list[2].get_max_width();

                        int required_bram1 = floor((double)LBrequired_plan/(double)ratio_list[1]);
                        int required_bram2 = floor((double)LBrequired_plan/(double)ratio_list[2]);

                        try_area = LBrequired_plan * ((35000/(double)ratio_list[0]*(ratio_list[0]-1)) + (40000/(double)ratio_list[0])) +
                                required_bram1 * (9000 + 5 * bram1_bits + 90 * sqrt((double)bram1_bits) + 600 * 2 * bram1_mwidth) +
                                required_bram2 * (9000 + 5 * bram2_bits + 90 * sqrt((double)bram2_bits) + 600 * 2 * bram2_mwidth);
                    }
//                    cout<<"try area this round is: "<<try_area<<endl;
//                    cout<<"cheapest area this round is: "<<cheapest_area<<endl;
                    if(try_area < cheapest_area){
                        if(try_area == 0){
                            cout<<"Something went wrong, area should not be zero!!!"<<endl;
                            exit(1);
                        }
                        cheapest_area = try_area;
                        cheapest_map_list.insert(cheapest_map_list.begin(),
                                                 mappedRam(logicram.get_lram_id(), mapper_id, num_luts, logicram.get_lram_depth(),
                                                           logicram.get_lram_width(), s, p, physical_candidate.get_pram_type(),
                                                           logicram.get_lram_mode(), curr_depth, curr_width, try_area));
                    }
                }// iterate through each combination of the same physical ram
            }// iterate through all physical candidates, we have found the cheapset map for this logic ram
            if(cheapest_map_list.empty()){
                cout<<"No available mapped result found!!!"<<endl;
                cout<<"Something went wrong"<<endl;
                exit(1);
            }
            mappedRam cheapest_map = cheapest_map_list[0];
            logic_circuit_list[circuit.get_circuit_id()].add_mapped_ram(cheapest_map);

            existing_LB += ceil((double)(cheapest_map.get_lut()) / 10.0);//MAGIC NUMBER HERE
            if(cheapest_map.get_map_type() == LUTRAM){
                used_lutram += cheapest_map.get_s() * cheapest_map.get_p();
            }else if(cheapest_map.get_map_type() == BRAM_CUSTOM_2){
                used_cusbram2 += cheapest_map.get_s() * cheapest_map.get_p();
            }else{
                used_cusbram += cheapest_map.get_s() * cheapest_map.get_p();
            }
            logicram_count++;
            if(logicram_count == circuit.get_ram_list().size()){
                circuit_areatested = cheapest_map.get_total_cost();
                if(circuit_areatested == 0){
                    cout<<"area got from mapped is zero!!!"<<endl;
                    cout<<"Something went wrong"<<endl;
                    exit(1);
                }
                //print for debug purpose: this current circuit info
                cout<<"circuit: "<<circuit.get_circuit_id()<<" used LUTRAM: "<<used_lutram
                    <<", used customed BRAM: "<<used_cusbram
                    <<", used customed BRAM2: "<<used_cusbram2
                    <<", my tested area is: "<<circuit_areatested<<endl;
            }
            if(op == NO_LUTRAM && used_lutram != 0){
                cout<<"Lutram used is not zero in this operation mode, which is not expected!!!"<<endl;
                cout<<"Something went wrong"<<endl;
                exit(1);
            }
            if(op == NO_LUTRAM || op == WITH_LUTRAM){
                if(used_cusbram2 != 0){
                    cout<<"Two types of bram are used in this operation mode, which is not expected!!!"<<endl;
                    cout<<"Something went wrong"<<endl;
                    exit(1);
                }
            }
        }// all logic ram in this circuit have been mapped
        logic_circuit_list[circuit.get_circuit_id()].set_circuit_area(circuit_areatested);
    }
}

