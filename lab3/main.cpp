
#include "circuit.h"


void parse_input(vector<circuit>& logic_circuit_list);
void debug_print(vector<circuit>& logic_circuit_list, vector<resource>& arc_resource_list);
void output_mapping(string output_file, vector<circuit>& logic_circuit_list, operationType op);
void output_my_area(string output_file, vector<circuit>& logic_circuit_list);

int main(int argc, char **argv) {

    if(argc < 2){
        cout<<"Cannot run the program due to the lack of arguments"<<endl;
        cout<<"Please refer to readme for details"<<endl;
        return 0;
    }

    clock_t cpu_start = clock();

    vector<circuit> logic_circuit_list;
    //parse the input txt file, store all circuits info in the structure
    parse_input(logic_circuit_list);

    operationType my_op;
    int input_arg = atoi(argv[1]);
    string mapping_outname;
    unsigned int bram_size, max_width, lb_ratio;
    vector<unsigned int> size_vec;
    vector<unsigned int> mwidth_vec;
    vector<unsigned int> bratio_vec;
    double lutram_ratio = 0.0;
    vector<unsigned int> ratio_map;
    if(input_arg == 1){
        cout<<"Running program for structure: STRATIX-IV"<<endl;
        my_op = STRATIX_IV;
        mapping_outname = "mapping_STRATXIV.txt";
    }else if(input_arg == 2 || input_arg == 3){
        if(argc < 5){
            cout<<"You need to pass more input as BRAM size, MAx Width, and ratio"<<endl;
            cout<<"Please refer to readme for details"<<endl;
            return 0;
        }

        size_vec.push_back(atoi(argv[2]));
        mwidth_vec.push_back(atoi(argv[3]));
        bratio_vec.push_back(atoi(argv[4]));
        if(input_arg == 2){
            cout<<"Running program for structure: NO LUTRAM with BRAM size: "<< size_vec[0]
                <<" with Max Width: "<<mwidth_vec[0]<<" with ratio: "<<bratio_vec[0]<<endl;
            my_op = NO_LUTRAM;
            mapping_outname = "mapping_NOLUTRAM_" + to_string(size_vec[0]) + "_" + to_string(mwidth_vec[0]) + "_" +
                              to_string(bratio_vec[0]) + ".txt";
        }else{
            cout<<"Running program for structure: WITH LUTRAM with BRAM size: "<< size_vec[0]
                <<" with Max Width: "<<mwidth_vec[0]<<" with ratio: "<<bratio_vec[0]<<endl;
            my_op = WITH_LUTRAM;
            mapping_outname = "mapping_WITHLUTRAM_" + to_string(size_vec[0]) + "_" + to_string(mwidth_vec[0]) + "_" +
                              to_string(bratio_vec[0]) + ".txt";
        }
        cout<<"my output name would be "<<mapping_outname<<endl;
    }else if(input_arg == 4){
        if(argc < 9){
            cout<<"You need to pass more input as lutram support ratio, BRAM size, MAx Width, and ratio"<<endl;
            cout<<"Please refer to readme for details"<<endl;
            return 0;
        }
        lutram_ratio = atoi(argv[2]);
        ratio_map.push_back(atoi(argv[2]));
        size_vec.push_back(atoi(argv[3]));
        mwidth_vec.push_back(atoi(argv[4]));
        bratio_vec.push_back(atoi(argv[5]));
        ratio_map.push_back(atoi(argv[5]));
        size_vec.push_back(atoi(argv[6]));
        mwidth_vec.push_back(atoi(argv[7]));
        bratio_vec.push_back(atoi(argv[8]));
        ratio_map.push_back(atoi(argv[8]));

        cout<<"Running program for structure: MB WITH LUTRAM with lutram support ratio: "<<lutram_ratio
        <<" BRAM size: "<< size_vec[0]<<" with Max Width: "<<mwidth_vec[0]<<" with ratio: "<<bratio_vec[0]
        <<" and BRAM size: "<< size_vec[1]<<" with Max Width: "<<mwidth_vec[1]<<" with ratio: "<<bratio_vec[1]<<endl;
        my_op = MB_WITH_LUTRAM;
        mapping_outname = "mapping_MBWITHLUTRAM_" + to_string(int(lutram_ratio))+"_"+
                to_string(size_vec[0]) + "_" + to_string(mwidth_vec[0]) + "_" +
                          to_string(bratio_vec[0]) + "_" + to_string(size_vec[1]) + "_" +
                          to_string(mwidth_vec[1]) + "_" + to_string(bratio_vec[1]) + ".txt";

    }
    else{
        cout<<"Cannot analyze the argument passed in."<<endl;
        cout<<"Please refer to readme for details"<<endl;
        return 0;
    }

    //prepare physical ram resource for the type of architecture that input specified
    vector<resource> arc_resource_list;
    construct_resource(arc_resource_list, my_op, input_parameter(size_vec, mwidth_vec, bratio_vec, lutram_ratio));

//    debug_print(logic_circuit_list, arc_resource_list);

    //perform the actual mapping
    if(my_op == STRATIX_IV){
        perform_basic_core_mapper(logic_circuit_list, arc_resource_list);
    }else{
        perform_custom_core_mapper(logic_circuit_list, arc_resource_list, my_op, ratio_map);
    }

    //for debug purpose, check two structures
//    debug_print(logic_circuit_list, arc_resource_list);

    //output the mapping file
    output_mapping(mapping_outname, logic_circuit_list, my_op);

    clock_t cpu_end = clock();
    double time_used = (cpu_end - cpu_start) / (CLOCKS_PER_SEC/1000);
    cout<<"CPU runtime of the program: "<<time_used<<endl;

    return 0;
}

void parse_input(vector<circuit>& logic_circuit_list){
    //---------------------parsing the txt file---------------
    string logic_rams_file = "./logical_rams.txt";
    string logic_bc_file = "./logic_block_count.txt";
    ifstream rams_fs(logic_rams_file);
    ifstream bc_fs(logic_bc_file);

    //get rid of the header line
    string header_line;
    getline(rams_fs, header_line);
    getline(rams_fs, header_line);
    getline(bc_fs, header_line);

    unsigned int bc_id, bc_num;
    while (bc_fs >> bc_id >> bc_num) {
        vector<logicRam> empty_list;
        vector<mappedRam> empty_mapped_list;
        circuit curr_circuit = circuit(bc_id, bc_num, empty_list, empty_mapped_list);
        logic_circuit_list.push_back(curr_circuit);
    }

    string lram_mode,circuit_number, lram_id, lram_d, lram_w;
    while (rams_fs >> circuit_number >> lram_id >> lram_mode >> lram_d >> lram_w) {
        ramMode actual_mode;
        if (lram_mode == "SimpleDualPort") {
            actual_mode = SimpleDualPort;
        } else if (lram_mode == "ROM") {
            actual_mode = ROM;
        } else if (lram_mode == "SinglePort") {
            actual_mode = SinglePort;
        } else {
            actual_mode = TrueDualPort;
        }
        unsigned int int_circuit_number = stoi(circuit_number);
        unsigned int int_lram_id = stoi(lram_id);
        unsigned int int_lram_d = stoi(lram_d);
        unsigned int int_lram_w = stoi(lram_w);

        logicRam curr_lr = logicRam(int_lram_id, actual_mode, int_lram_d, int_lram_w);
        logic_circuit_list[int_circuit_number].add_logic_ram(curr_lr);
    }
}

void output_mapping(string output_file, vector<circuit>& logic_circuit_list, operationType op){
    ofstream outs;
    outs.open(output_file);
    for(auto& circuit: logic_circuit_list){
        int circuit_id = circuit.get_circuit_id();
        for(auto& mapped: circuit.get_mapped_list()){
            //need to adjust type printed
            if(mapped.get_map_type() == BRAM_CUSTOM){
                if(op == NO_LUTRAM){
                    mapped.change_type(LUTRAM); //get type = 1
                }else{
                    mapped.change_type(BRAM_8192); //get type = 2
                }
            }else if(mapped.get_map_type() == BRAM_CUSTOM_2){
                mapped.change_type(BRAM_128K); //get type = 3
            }
            outs<<circuit_id<<" "<<mapped.get_ram_id()<<" "<<mapped.get_lut()<<" LW "<<mapped.get_lwidth()<<" LD "
            <<mapped.get_ldepth()<<" ID "<<mapped.get_mapper_id()<<" S "<<mapped.get_s()<<" P "<<mapped.get_p()
            <<" Type "<<mapped.get_map_type()<<" Mode "<<mapped.get_lram_mode()<<" W "<<mapped.get_pwidth()
            <<" D "<<mapped.get_pdepth()<<"\n";
        }
    }
    outs.close();
}

void output_my_area(string output_file, vector<circuit>& logic_circuit_list){
    ofstream outs;
    outs.open(output_file);
    for(auto& circuit: logic_circuit_list){
        int circuit_id = circuit.get_circuit_id();
//        for(auto& mapped: circuit.get_mapped_list()){
//            outs<<circuit_id<<" "<<mapped.get_ram_id()<<" "<<mapped.get_lut()<<" LW "<<mapped.get_lwidth()<<" LD "
//                <<mapped.get_ldepth()<<" ID "<<mapped.get_mapper_id()<<" S "<<mapped.get_s()<<" P "<<mapped.get_p()
//                <<" Type "<<mapped.get_map_type()<<" Mode "<<mapped.get_lram_mode()<<" W "<<mapped.get_pwidth()
//                <<" D "<<mapped.get_pdepth()<<"\n";
//        }
        outs<<circuit_id<<" "<<circuit.get_circuit_area()<<"\n";
    }
    outs.close();
}

void debug_print(vector<circuit>& logic_circuit_list, vector<resource>& arc_resource_list){
//    std::cout << "As a midpoint check here:" << std::endl;
//    std::cout << "print the circuit 37's number of lb: " << logic_circuit_list[37].get_circuit_num_lb() <<std::endl;
//    std::cout << "As a midpoint check here:" << std::endl;
//    int i;
//    for (i = 0; i < 69; i++)
//    {
//        std::cout << "print the circuit "<<i<<"s number of lb: " << logic_circuit_list[i].get_circuit_num_lb() <<std::endl;
//        cout<<"this circuit has how many logic rams? "<<(logic_circuit_list[i].get_ram_list()).size()<<endl;
//    }
//    //59	604	SinglePort    	512	8
//    std::cout << "print the circuit 59, RAM id = 604, type: " << (logic_circuit_list[59].get_ram_list())[604].get_lram_mode() <<std::endl;
//    std::cout << "print the circuit 59, RAM id = 604, width: " << (logic_circuit_list[59].get_ram_list())[604].get_lram_depth() <<std::endl;

    cout <<"Check resource list:"<<endl;
    for(int i = 0; i < arc_resource_list.size(); i++){
        cout<<"---------"<<i<<"----------"<<endl;
        cout<<"Resource "<< i << " has type as: " <<arc_resource_list[i].get_pram_type()<< endl;
        cout<<"it has ratio: "<<arc_resource_list[i].get_ratio()<<endl;
        cout<<"it max depth: "<<arc_resource_list[i].get_max_depth()<<", max width: "<<arc_resource_list[i].get_max_width()
            <<" and size is: "<<arc_resource_list[i].get_pram_size()<<endl;
        vector<pair<unsigned int, unsigned int> > test_comb = arc_resource_list[i].get_comb_list();
        for(int j = 0; j < test_comb.size(); j++){
            cout<<"combination: {"<<test_comb[j].first<<", "<<test_comb[j].second<<"}"<<endl;
        }
    }
//    cout <<"Check logic circuit list:"<<endl;
//    for (int i = 0; i < 69; i++)
//    {
//        std::cout << "print the circuit "<<i<<"s number of lb: " << logic_circuit_list[i].get_circuit_num_lb() <<std::endl;
//        cout<<"this circuit has how many logic rams? "<<(logic_circuit_list[i].get_ram_list()).size()<<endl;
//        cout<<"I calculated area is!!"<<setprecision (10)<<logic_circuit_list[i].get_circuit_area()<<endl;
//    }
}




