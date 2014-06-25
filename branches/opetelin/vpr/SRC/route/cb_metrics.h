#ifndef CB_METRICS_H
#define CB_METRICS_H

#include <vector>
#include <set>

#define MAX_OUTER_ITERATIONS 100000
#define MAX_INNER_ITERATIONS 10
#define INITIAL_TEMP 1
#define LOWEST_TEMP 0.00001
#define TEMP_DECREASE_FAC 0.999




/**** Enums ****/
/* Defines the different kinds of metrics that we can adjust */
enum e_metric{
	WIRE_HOMOGENEITY = 0,
	HAMMING_PROXIMITY,
	LEMIEUX_COST_FUNC,	/* described by lemieux in his 2001 book; used there for creating routable sparse crossbars */
	NUM_WIRE_METRICS,
	PIN_DIVERSITY,
	NUM_METRICS
};


/**** Typedefs ****/
/* 2D vector of integers */
typedef std::vector< std::vector<int> > t_2d_int_vec;
/* 3D vector of integers */
typedef std::vector< std::vector< std::vector<int> > > t_3d_int_vec;

/* a vector of vectors of integer sets. used for pin-to-track and track-to-pin lookups */
typedef std::vector< std::vector< std::set<int> > > t_vec_vec_set;


/**** Classes ****/
/* constructor and destructor for a 2d array */
class int_array_2d{
private:	
	bool already_set;
	int sizex;
	int sizey;

public:
	int **ptr;

	int_array_2d(){
		sizex = 0;
		sizey = 0;
		already_set = false;
	}
	int_array_2d(int x, int y){
		sizex = x;
		sizey = y;
		ptr = (int **) alloc_matrix(0, x-1, 0, y-1, sizeof(int));
		already_set = true;
	}
	
	/* destructor takes care of memory clean up */
	~int_array_2d(){
		if (already_set){
			free_matrix(ptr, 0, sizex-1, 0, sizeof(int));
		}
	}

	/* can allocate memory for array if that hasn't been done already */
	void alloc_array(int x, int y){
		free_array();
	
		if (!already_set){
			sizex = x;
			sizey = y;
			ptr = (int **) alloc_matrix(0, x-1, 0, y-1, sizeof(int));
			already_set = true;
		}
	}
	void free_array(){
		if (already_set){
			free_matrix(ptr, 0, sizex-1, 0, sizeof(int));
			ptr = NULL;
			already_set = false;
		}
		sizex = sizey = 0;
	}
};

/* constructor and destructor for a 3d array */
class int_array_3d{
private:	
	bool already_set;
	int sizex;
	int sizey;
	int sizez;

public:
	int ***ptr;

	int_array_3d(){
		sizex = 0;
		sizey = 0;
		sizez = 0;
		already_set = false;
		ptr = NULL;
	}
	int_array_3d(int x, int y, int z){
		sizex = x;
		sizey = y;
		sizez = z;
		ptr = (int ***) alloc_matrix3(0, x-1, 0, y-1, 0, z-1, sizeof(int));
		already_set = true;
	}
	
	/* destructor takes care of memory clean up */
	~int_array_3d(){
		if (already_set){
			assert(NULL != ptr);
			free_matrix3(ptr, 0, sizex-1, 0, sizey-1, 0, sizeof(int));
		}
	}
	/* can allocate memory for array if that hasn't been done already */
	void alloc_array(int x, int y, int z){
		free_array();

		if (!already_set){
			sizex = x;
			sizey = y;
			sizez = z;
			ptr = (int ***) alloc_matrix3(0, x-1, 0, y-1, 0, z-1, sizeof(int));
			already_set = true;

			//init_to_zero();
		}
	}
	void free_array(){
		if (already_set){
			free_matrix3(ptr, 0, sizex-1, 0, sizey-1, 0, sizeof(int));
			ptr = NULL;
			already_set = false;
		}
		sizex = sizey = sizez = 0;
	}
};


class Conn_Block_Metrics{
public:
	/* the actual metrics */
	float pin_diversity;
	float wire_homogeneity;
	float hamming_proximity;
	float lemieux_cost_func;

	int num_wire_types;			/* the number of different wire types, used for computing pin diversity */
	t_2d_int_vec pin_locations;		/* [0..3][0..num_on_this_side-1]. Keeps track of which pins come out on which side of the block */

	/* these vectors simplify the calculation of the various metrics */
	t_vec_vec_set track_to_pins;		/* [0..3][0..W-1][0..pins_connected-1]. A convenient lookup for which pins connect to a given track */
	t_vec_vec_set pin_to_tracks;		/* [0..3][0..num_pins_on_side-1][0..tracks_connected-1]. A lookup for which tracks connect to a given pin */
	t_3d_int_vec wire_types_used_count;	/* [0..3][0..num_pins_on_side-1][0..num_wire_types-1]. Keeps track of how many times each pin connects to each of the wire types */

	void clear(){
		pin_diversity = wire_homogeneity = hamming_proximity = lemieux_cost_func = 0.0;
		num_wire_types = 0;
		pin_locations.clear();
		track_to_pins.clear();
		pin_to_tracks.clear();
		wire_types_used_count.clear();
	}
};

/**** Structures ****/
/* Contains statistics on a connection block's diversity */
typedef struct s_conn_block_homogeneity {
	/* actual metrics */
	float pin_homogeneity;
	float pin_diversity;
	float wire_homogeneity;
	float hamming_distance;
	float hamming_proximity;

	/*other important data -- very useful for computing metrics within an optimizer */
	int num_wire_types;
	int_array_2d wh_wire_conns;
	int_array_2d ph_pin_averages;
	int_array_3d hd_pin_array;
	int counted_pins_per_side[4];

	/* initialize to zeros */
	s_conn_block_homogeneity() : 
	wh_wire_conns(), ph_pin_averages(), hd_pin_array()
	{
		pin_homogeneity = 0;
		pin_diversity = 0;
		wire_homogeneity = 0;
		hamming_distance = 0;
		hamming_proximity = 0;

		num_wire_types = 0;
		for (int i = 0; i<4; i++)
			counted_pins_per_side[i] = 0;
	}
} t_conn_block_homogeneity;


int get_num_wire_types(INP int num_segments, INP t_segment_inf *segment_inf);

void get_conn_block_homogeneity(OUTP t_conn_block_homogeneity &cbm, INP t_type_ptr block_type, 
		INP int *****tracks_connected_to_pin, INP e_pin_type pin_type, 
		INP int *Fc_array, INP int nodes_per_chan, INP int num_segments, 
		INP t_segment_inf *segment_inf);

/* TODO: this function seems to not be playing well with the pointers used by the metric structure
   investigate */
t_conn_block_homogeneity get_conn_block_homogeneity_fpga(INP t_conn_block_homogeneity *block_homogeneity, 
		INP int num_block_types, INP struct s_grid_tile **fpga_grid, INP int size_x, INP int size_y,
		INP t_type_ptr block_types, INP e_pin_type pin_type);

void adjust_pin_metric(INP float PD_target, INP float PD_tolerance, INP float HD_tolerance,
		INP t_type_ptr block_type, 
		INOUTP int *****tracks_connected_to_pin, INP e_pin_type pin_type, 
		INP int *Fc_array, INP int nodes_per_chan, INP int num_segments, 
		INP t_segment_inf *segment_inf);

void adjust_hamming(INP float target, INP float target_tolerance, INP float pin_tolerance,
		INP t_type_ptr block_type, INOUTP int *****tracks_connected_to_pin, 
		INP e_pin_type pin_type, INP int *Fc_array, INP int nodes_per_chan, 
		INP int num_segments, INP t_segment_inf *segment_inf);

void generate_random_trackmap(INOUTP int *****tracks_connected_to_pin, INP e_pin_type pin_type, 
		INP int Fc, INP int nodes_per_chan, INP t_type_ptr block_type);

void write_trackmap_to_file(INP char *filename, INP int *****tracks_connected_to_pin, INP e_pin_type pin_type,
		INP t_type_ptr block_type, INP int Fc);

void read_trackmap_from_file(INP char *filename, OUTP int *****tracks_connected_to_pin, INP e_pin_type pin_type,
		INP t_type_ptr block_type, INP int Fc);

int get_max_Fc(INP int *Fc_array, INP t_type_ptr block_type, INP e_pin_type pin_type);

/* adjusts the connection block until the appropriate wire metric has hit it's target value. the pin metric is kept constant
   within some tolerance */
/* calculates all the connection block metrics and returns them through the cb_metrics variable */
void get_conn_block_metrics(INP t_type_ptr block_type, INP int *****tracks_connected_to_pin, INP int num_segments, INP t_segment_inf *segment_inf, 
		INP e_pin_type pin_type, INP int num_pin_type_pins, INP int nodes_per_chan, INP int Fc, 
		INOUTP Conn_Block_Metrics *cb_metrics);

/* adjusts the connection block until the appropriate wire metric has hit it's target value. the pin metric is kept constant
   within some tolerance */
void adjust_cb_metric(INP e_metric metric, INP float target, INP float target_tolerance, INP float pin_tolerance,
		INP t_type_ptr block_type, INOUTP int *****pin_to_track_connections, 
		INP e_pin_type pin_type, INP int *Fc_array, INP int nodes_per_chan, 
		INP int num_segments, INP t_segment_inf *segment_inf);
#endif /*CB_METRICS_H*/
