#ifndef LAB3_IMPLEMENTATION_CIRCUIT_H
#define LAB3_IMPLEMENTATION_CIRCUIT_H

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <tuple>
#include <utility>
#include <float.h>
#include <ctime>
#include <cmath>

using namespace std;

enum ramMode {
    SimpleDualPort = 1,
    ROM,
    SinglePort,
    TrueDualPort
};

enum arch_type{
    LUTRAM = 1,
    BRAM_8192,
    BRAM_128K,
    BRAM_CUSTOM,
    BRAM_CUSTOM_2
};

enum operationType {
    STRATIX_IV = 1,
    NO_LUTRAM,
    WITH_LUTRAM,
    MB_WITH_LUTRAM
};

class input_parameter{
private:
    vector<unsigned int> bram_size;
    vector<unsigned int> bram_mwidth;
    vector<unsigned int> bram_ratio;
    double lutram_ratio;
public:
    input_parameter(vector<unsigned int> i_size, vector<unsigned int> i_mwidth, vector<unsigned int> i_bratio, double
    i_lratio){
        bram_size = i_size;
        bram_mwidth = i_mwidth;
        bram_ratio = i_bratio;
        lutram_ratio = i_lratio;
    }
    vector<unsigned int> get_input_bram_size(){return bram_size;}
    vector<unsigned int> get_input_bram_mwidth(){return bram_mwidth;}
    vector<unsigned int> get_input_bram_ratio(){return bram_ratio;}
    double get_input_lutram_ratio(){return lutram_ratio;}
};

class mappedRam {
private:
    int ram_id;
    int mappedram_id;
    unsigned int additional_lut;
    unsigned int logic_depth;
    unsigned int logic_width;
    unsigned int serial;
    unsigned int parallel;
    arch_type type;
    string lram_mode;
    unsigned int mapped_depth;
    unsigned int mapped_width;
    double cost;
public:
    mappedRam(int i_ram_id, int i_mapper_id, unsigned int i_addlut, unsigned int i_ld, unsigned int i_lw,
            unsigned int i_s, unsigned int i_p, arch_type i_type, string i_lram_mode, unsigned int i_mapd,
            unsigned int i_mapw, double i_area){
        ram_id = i_ram_id;
        mappedram_id = i_mapper_id;
        additional_lut = i_addlut;
        logic_depth = i_ld;
        logic_width = i_lw;
        serial = i_s;
        parallel = i_p;
        type = i_type;
        lram_mode = i_lram_mode;
        mapped_depth = i_mapd;
        mapped_width = i_mapw;
        cost = i_area;
    }
    void change_type(arch_type format_type){type = format_type;}
    int get_ram_id(){return ram_id;}
    int get_mapper_id(){return mappedram_id;}
    unsigned int get_lut(){return additional_lut;}
    unsigned int get_ldepth(){return logic_depth;}
    unsigned int get_lwidth(){return logic_width;}
    unsigned int get_s(){return serial;}
    unsigned int get_p(){return parallel;}
    arch_type get_map_type(){return type;}
    string get_lram_mode(){return lram_mode;}
    unsigned int get_pdepth(){return mapped_depth;}
    unsigned int get_pwidth(){return mapped_width;}
    double get_total_cost(){return cost;}
};

class logicRam {
private:
    unsigned int ram_id;
    ramMode mode;
    unsigned int depth;
    unsigned int width;
public:
    logicRam(unsigned int i_id, ramMode i_mode, unsigned int i_depth, unsigned int i_width){
        ram_id = i_id;
        mode = i_mode;
        depth = i_depth;
        width = i_width;
    }
    unsigned int get_lram_id() { return ram_id; }
    unsigned int get_lram_depth() { return depth; }
    unsigned int get_lram_width() { return width; }
    string get_lram_mode();
};


class circuit {
private:
    unsigned int circuit_id;
    unsigned int num_lb;
    vector<logicRam> logic_ram_list;
    vector<mappedRam> mapped_ram_list;
    double tested_area;
public:
    circuit(unsigned int i_id, unsigned int i_num, vector<logicRam> i_list, vector<mappedRam> i_mapped_list){
        circuit_id = i_id;
        num_lb = i_num;
        logic_ram_list = i_list;
        mapped_ram_list = i_mapped_list;
    }
    void add_logic_ram(logicRam i_ram){
        logic_ram_list.push_back(i_ram);
    }
    void add_mapped_ram(mappedRam i_mapped){
        mapped_ram_list.push_back(i_mapped);
    }
    void set_circuit_area(double i_area) {tested_area = i_area;}
    unsigned int get_circuit_id() { return circuit_id; }
    unsigned int get_circuit_num_lb() { return num_lb; }
    vector<logicRam> get_ram_list(){ return logic_ram_list;}
    vector<mappedRam> get_mapped_list(){ return mapped_ram_list;}
    double get_circuit_area(){return tested_area;}
};

extern vector<circuit> logic_circuit_list;

class resource{
private:
    arch_type physical_ram;
    double ratio_to_block;
    unsigned int max_depth;
    unsigned int max_width;
    unsigned int size;
    vector<pair<unsigned int, unsigned int> > possible_comb;
public:
    resource(arch_type i_pram, double i_ratio, unsigned int i_mdepth, unsigned int i_mwidth,
            unsigned int i_size, vector<pair<unsigned int, unsigned int> > i_comb_list){
        physical_ram = i_pram;
        ratio_to_block = i_ratio;
        max_depth = i_mdepth;
        max_width = i_mwidth;
        size = i_size;
        possible_comb = i_comb_list;
    }

    void add_dw_combination(pair<unsigned int, unsigned int> i_comb){
        possible_comb.push_back(i_comb);
    }
    void prepare_combination();
    arch_type get_pram_type() { return physical_ram; }
    double get_ratio() { return ratio_to_block; }
    unsigned int get_max_depth() { return max_depth; }
    unsigned int get_max_width() { return max_width; }
    unsigned int get_pram_size() { return size; }
    vector<pair<unsigned int, unsigned int> > get_comb_list(){ return possible_comb;}
};

extern vector<resource> arch_resource_list;

void construct_resource(vector<resource>& resource_list, operationType op, input_parameter input_pack);

void perform_basic_core_mapper(vector<circuit>& logic_circuit_list, vector<resource>& resource_list);

void perform_custom_core_mapper(vector<circuit>& logic_circuit_list, vector<resource>& resource_list, operationType op,
        vector<unsigned int>ratio_list);


#endif //LAB3_IMPLEMENTATION_CIRCUIT_H
