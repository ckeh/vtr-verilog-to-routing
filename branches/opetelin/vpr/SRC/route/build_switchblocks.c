/*
DESCRIPTION
The functions in this file are used to help build the switchblocks in rr_graph.c:build_rr_graph
(partially) based on the information in physical_types.h:t_switchblock_inf parsed from the switchblock section
of the XML architecture file.
*/

/* TODO TODO TODO!! in t_seg_details he referes to 'group' as a collection of L tracks that are consecutive and all 
			all with different start points. I'm referring to 'group' as the collection of tracks 
			in a channel going in the same direction and having the same start point (going by literature,
			this seems to be the more appropriate definition of group) */

#include <cstring>
#include <cassert>
#include <algorithm>
#include <iterator>
#include "build_switchblocks.h"
#include "physical_types.h"
#include "parse_switchblocks.h"

using namespace std;

/**** Typedefs ****/
/* Used for translating the name of a track type to the number of tracks belonging to this type */
typedef std::map< std::string, int > t_track_type_sizes;


/**** Function Declarations ****/
/* Counts the number of tracks in each track type in the specified channel */
static void count_track_type_sizes(INP t_seg_details *channel, INP int nodes_per_chan, 
			INOUTP t_track_type_sizes *track_type_sizes);
//TODO: comment
static void compute_track_connections(INP int x_coord, INP int y_coord, INP enum e_side from_side,
			INP enum e_side to_side, INP int from_track, INP t_chan_details * chan_details_x,
			INP t_chan_details * chan_details_y, INP t_switchblock_inf *sb,
			INP int nx, INP int ny, INP int nodes_per_chan, INP t_track_type_sizes &track_type_sizes,
			INP e_directionality directionality, INOUTP t_sb_connection_map *sb_conns);

static t_rr_type index_into_correct_chan(INP int tile_x, INP int tile_y, INP enum e_side side, 
			INP t_chan_details *chan_details_x, INP t_chan_details *chan_details_y,
			OUTP int *chan_x, OUTP int *chan_y, 
			OUTP t_chan_details **chan_details);

/* checks whether the specified coordinates are out of bounds */
static bool coords_out_of_bounds(INP int nx, INP int ny, INP int x_coord, INP int y_coord, 
			e_rr_type chan_type);

/* returns seg coordinate based on a channel's x/y coordinates */
static int get_seg_coordinate(INP t_rr_type chan_type, INP int x_coord, INP int y_coord);

/* returns the direction in which a destination track at the specified
   switchblock side should be headed */
static e_direction get_dest_direction(e_side to_side, e_directionality directionality);

/* returns the direction in which a source track at the specified
   switchblock side should be headed. */
static e_direction get_src_direction(e_side from_side, e_directionality directionality);

/* returns the group number of the specified track */
static int get_track_group(INP t_seg_details &track_details, int seg_coord);

/* Finds the index of the first track in the channel specified by 'track_details'
   to have the type name specified by 'type' */
static int find_start_of_track_type(INP t_seg_details *track_details, INP const char *type, 
			INP int nodes_per_chan, INP t_track_type_sizes track_type_sizes);

/* returns the track index in the channel specified by 'track_details' that corresponds to the 
   first occurence of the specified track group (belonging to type referred to by track_type_start, 
   track_type_size, and track_direction) */
static int find_start_of_track_group(INP t_seg_details *track_details, INP const char *type, 
			INP int track_group, INP int track_seg_coord, INP e_direction track_direction, 
			INP int track_type_start, INP int nodes_per_chan, INP t_track_type_sizes track_type_sizes);

/* fills the 'tracks_in_group' vector with the indices of the tracks in the 'track_details' channel
   which have the specified 'track_type', 'track_group', and are going in 'track_direction' */
static void get_group_tracks(INP t_seg_details *track_details, INP const char *track_type, INP int track_group,
			INP int track_seg_coord, INP e_direction track_direction, INP int nodes_per_chan, 
			INP t_track_type_sizes track_type_sizes, INOUTP vector<int> *tracks_in_group);

///* checks whether the entry specified by coord exists in the switchblock map sb_conns */
//static bool sb_connection_exists( INP Switchblock_Lookup coord, INP t_sb_connection_map *sb_conns);



/**** Function Definitions ****/
t_sb_connection_map * alloc_and_load_switchblock_permutations( INP t_chan_details * chan_details_x, 
				INP t_chan_details * chan_details_y, INP int nx, INP int ny, 
				INP vector<t_switchblock_inf> switchblocks, 
				INP int nodes_per_chan, INP e_directionality directionality){

	Switchblock_Lookup coord;

	/* the switchblock permutations pointer */
	t_sb_connection_map *sb_conns = new t_sb_connection_map;

	/* We assume that x & y channels have the same ratios of track types. i.e., looking at a single 
	   channel is representative of all channels in the FPGA -- as of 3/9/2013 this is true in VPR */
	t_track_type_sizes track_type_sizes;
	count_track_type_sizes(chan_details_x[0][0], nodes_per_chan, &track_type_sizes);
	
	/* iterate over all the switchblocks specified in the architecture */
	for (int i_sb = 0; i_sb < (int)switchblocks.size(); i_sb++){
		t_switchblock_inf sb = switchblocks[i_sb];

		/* verify that switchblock type matches specified directionality -- currently we have to stay consistent */
		if (directionality != sb.directionality){
			vpr_printf(TIO_MESSAGE_ERROR, "alloc_and_load_switchblock_connections: Switchblock %s does not match directionality of architecture\n", sb.name.c_str());
			exit(1);
		}

		/* Iterate over the x,y coordinates spanning the FPGA. Currently, the FPGA size is set as
		   (0..nx+1) by (0..ny+1), so we iterate over 0..nx and 0..ny. */
		for (int x_coord = 0; x_coord <= nx+1; x_coord++){
			for (int y_coord = 0; y_coord <= ny+1; y_coord++){
				/* Iterate over each track in channel */
				for (int itrack = 0; itrack < nodes_per_chan; itrack++){

					/* now we iterate over all the potential side1->side2 connections */
					for ( e_side from_side = (e_side) 0; from_side < 4; from_side = (e_side)(from_side + 1) ){
						for ( e_side to_side = (e_side) 0; to_side < 4; to_side = (e_side)(to_side + 1) ){
							
							/* Fill appropriate entry of the sb_conns map with vector specifying the tracks 
							   the current track will connect to */
							compute_track_connections(x_coord, y_coord, from_side, to_side, itrack,
									chan_details_x, chan_details_y, &sb, nx, ny, nodes_per_chan,
									track_type_sizes, directionality, sb_conns);
							
						}
					}
				}
			}
		}
	}
	//assert(false);
	return sb_conns;
}

/* calls delete on the switchblock permutations pointer and sets it to 0 */
void free_switchblock_permutations(INOUTP t_sb_connection_map *sb_conns){
	delete sb_conns;
	sb_conns = NULL;
	return;
}

/* Counts the number of tracks in each track type in the specified channel */
static void count_track_type_sizes(INP t_seg_details *channel, INP int nodes_per_chan, 
			INOUTP t_track_type_sizes *track_type_sizes){
	string track_type;
	const char * new_type;
	int num_tracks = 0;	

	track_type = channel[0].type_name_ptr;
	for (int itrack = 0; itrack < nodes_per_chan; itrack++){
		new_type = channel[itrack].type_name_ptr;
		if (strcmp(new_type, track_type.c_str()) != 0){
			(*track_type_sizes)[track_type] = num_tracks;
			track_type = new_type;
			num_tracks = 0;
		}
		num_tracks++;
	}
	(*track_type_sizes)[track_type] = num_tracks;

	return;
} 

//TODO: comment
static void compute_track_connections(INP int x_coord, INP int y_coord, INP enum e_side from_side,
			INP enum e_side to_side, INP int from_track, INP t_chan_details * chan_details_x,
			INP t_chan_details * chan_details_y, INP t_switchblock_inf *sb,
			INP int nx, INP int ny, INP int nodes_per_chan, INP t_track_type_sizes &track_type_sizes,
			INP e_directionality directionality, INOUTP t_sb_connection_map *sb_conns){
	
	t_chan_details *from_chan_details = NULL;	/* details for source channel */
	t_chan_details *to_chan_details = NULL ;	/* details for destination channel */
	int from_x, from_y;				/* index into source channel */
	int to_x, to_y;					/* index into destination channel */
	t_rr_type from_chan_type, to_chan_type;		/* the type of channel - i.e. CHANX or CHANY */
	e_direction src_direction, dest_direction;	/* the direction in which the source and destination tracks head */

	Connect_SB_Sides side_conn(from_side, to_side);		/* for indexing into this switchblock's permutation funcs */
	Switchblock_Lookup sb_coord(x_coord, y_coord, from_side, to_side, from_track);	/* for indexing into FPGA's switchblock map */

	/* can't connect a switchblock side to itself */
	if (from_side == to_side){
		return;
	}
	/* check that the permutation map has an entry for this side combination */
	t_permutation_map::const_iterator map_it = sb->permutation_map.find(side_conn);
	if (sb->permutation_map.end() == map_it){
		/* the specified switchblock does not have any permutation funcs for this side1->side2 connection */
		return;
	}

	/* find the correct channel, and the coordinates to index into it for both the source and
	   destination channels */
	from_chan_type = index_into_correct_chan(x_coord, y_coord, from_side, chan_details_x, chan_details_y, 
				&from_x, &from_y, &from_chan_details);
	to_chan_type = index_into_correct_chan(x_coord, y_coord, to_side, chan_details_x, chan_details_y, 
				&to_x, &to_y, &to_chan_details);

	src_direction = get_src_direction(from_side, directionality);
	dest_direction = get_dest_direction(to_side, directionality);
	
	/* make sure from_x/y and to_x/y aren't out of bounds */
	if (coords_out_of_bounds(nx, ny, to_x, to_y, to_chan_type) ||
	    coords_out_of_bounds(nx, ny, from_x, from_y, from_chan_type)){
		return;
	}

	/* iterate over all the wire connections specified by this switchblock */
	t_wireconn_inf *wireconn_ptr = NULL;
	const char *track_name = from_chan_details[from_x][from_y][from_track].type_name_ptr;
	for (int iconn = 0; iconn < (int)sb->wireconns.size(); iconn++){
		wireconn_ptr = &(sb->wireconns[iconn]);
		
		/* name of track type we're connecting from/to */
		const char *from_type = wireconn_ptr->from_type.c_str();
		const char *to_type = wireconn_ptr->to_type.c_str();
		/* the 'seg' coordinates of the from/to channels */
		int from_seg = get_seg_coordinate( from_chan_type, from_x, from_y );
		int to_seg = get_seg_coordinate( to_chan_type, to_x, to_y );
		/* the group of the source and destination tracks */
		int from_group = get_track_group(from_chan_details[from_x][from_y][from_track], from_seg);
		int to_group = wireconn_ptr->to_group;
		/* vectors that will contain indices of the source/destination tracks in the source/destination channel */
		vector<int> src_tracks_group;
		vector<int> dest_tracks_group;
		/* the index of the source/destination tracks within their own group */
		int src_track_in_group, dest_track_in_group;
		/* the effective channel width is the size of the destination track type->group */
		int dest_W;

		/* check that the current track is a source of this wire connection */
		/* check the type */
		if ( strcmp(track_name, from_type) != 0 ){	
			continue;
		}
		/* check the group */
		if ( from_group != wireconn_ptr->from_group ){
			continue;
		}

		/* now we need to find all tracks in the destination channel that correspond to the 
		   type and group specified by this wireconn. Must also go in appropriate 
		   direction (from e_direction) */

		/* get the indices of the destination tracks, as well as the effective destination 
		   channel width (which is the number of destination tracks */
		get_group_tracks(to_chan_details[to_x][to_y], to_type, to_group, to_seg, dest_direction,
						nodes_per_chan, track_type_sizes, &dest_tracks_group);
		dest_W = dest_tracks_group.size();

		/* get the index of the source track within it's own group */
		get_group_tracks(from_chan_details[from_x][from_y], from_type, from_group, from_seg, src_direction,
						nodes_per_chan, track_type_sizes, &src_tracks_group);
		vector<int>::iterator it = find(src_tracks_group.begin(), src_tracks_group.end(), from_track);
		src_track_in_group = it - src_tracks_group.begin();
		
		/* get a reference to the vector containing desired side1->side2 permutations */
		vector<string> &permutations = sb->permutation_map[side_conn];

		/* iterate over the permutation functions. */
		int to_track;
		s_formula_data formula_data;
		for (int iperm = 0; iperm < (int)permutations.size(); iperm++){
			/* Convert the symbolic permutation formula to a number */
			formula_data.dest_W = dest_W;
			formula_data.track = src_track_in_group;
			dest_track_in_group = get_sb_formula_result(permutations[iperm].c_str(), formula_data);
			dest_track_in_group %= dest_W;
			/* the resulting track number is the *index* of the destination track in it's own
			   group, so convert that back to the absolute index of the track in the channel */
			to_track = dest_tracks_group.at(dest_track_in_group);
			
			//Everything seems to be working OK here...
			//printf("W: %d  from_s: %d  to_s: %d  tile_x: %d  tile_y: %d  from_x: %d  from_y: %d  to_x: %d  to_y: %d  from_trk: %d  src: %d  dest: %d  dest_W: %d  to_trk: %d\n", nodes_per_chan, from_side, to_side, x_coord, y_coord, from_x, from_y, to_x, to_y, from_track, src_track_in_group, dest_track_in_group, dest_W, to_track);
		
			/* and now, finally, add this switchblock connection to the switchblock connections map */
			(*sb_conns)[sb_coord].push_back(to_track);
			if (BI_DIRECTIONAL == directionality){
				/* in a bidirectional architecture, the destination track will also connect to
				   to the source track via a reverse connection */
				Switchblock_Lookup sb_coord_reverse(x_coord, y_coord, to_side, from_side, to_track);
				(*sb_conns)[sb_coord_reverse].push_back(from_track);
			}
		}
	}

	track_name = NULL;
	wireconn_ptr = NULL;
	//iterate over all wireconns
		//confirm wires with specified names exist
		//find specified source and target tracks
		//connect them according to the specific permutation function
		//if bidirectional, this is where we set the reverse connection
	
	from_chan_details = NULL;
	to_chan_details = NULL;
	return;
}


/* Here we find the correct channel (x or y), and the coordinates to index into it based on the 
   specified tile coordinates and the switchblock side. Also returns the type of channel
   that we are indexing into (ie, CHANX or CHANY */
static t_rr_type index_into_correct_chan(INP int tile_x, INP int tile_y, INP enum e_side side, 
			INP t_chan_details *chan_details_x, INP t_chan_details *chan_details_y,
			OUTP int *set_x, OUTP int *set_y, 
			OUTP t_chan_details **set_chan_details){
	
	t_rr_type chan_type;

	/* here we use the VPR convention that a tile 'owns' the channels directly to the right
	   and above it */
	switch (side){
		case TOP:
			*set_chan_details = chan_details_y;
			*set_x = tile_x;
			*set_y = tile_y+1;
			chan_type = CHANY;
			break;
		case RIGHT:
			*set_chan_details = chan_details_x;
			*set_x = tile_x+1;
			*set_y = tile_y;
			chan_type = CHANX;
			break;
		case BOTTOM:
			/* this is y-channel on the right of the tile */
			*set_chan_details = chan_details_y;
			*set_x = tile_x;
			*set_y = tile_y;
			chan_type = CHANY;
			break;
		case LEFT:
			/* this is x-channel on top of the tile */
			*set_chan_details = chan_details_x;
			*set_x = tile_x;
			*set_y = tile_y;
			chan_type = CHANX;
			break;
		default:
			vpr_printf(TIO_MESSAGE_ERROR, "index_into_correct_chan: unknown side specified: %d\n", side);
			exit(1);
			break;
	}

	return chan_type;
}

/* checks whether the specified coordinates are out of bounds */
static bool coords_out_of_bounds(INP int nx, INP int ny, INP int x_coord, INP int y_coord, 
			e_rr_type chan_type){
	bool result = true;

	if (CHANX == chan_type){
		if (x_coord <=0 || x_coord >= nx+1 ||	/* there is no x-channel at x=0 */
		    y_coord < 0 || y_coord >= ny+1){
			result = true;
		} else {
			result = false;
		}
	} else if (CHANY == chan_type){
		if (x_coord < 0 || x_coord >= nx+1 ||
		    y_coord <= 0 || y_coord >= ny+1){	/* there is no y-channel at y=0 */
			result = true;
		} else {
			result = false;
		}

	} else {
		vpr_printf(TIO_MESSAGE_ERROR, "coords_out_of_bounds(): illegal channel type %d\n", chan_type);
		exit(1);
	}
	return result;
}

/* We can look at a channel as either having an x and y coordinate, or as having a 
   'seg' and 'chan' coordinate. Here we return the seg coordinate based on the 
   channel type and channel x/y coordinates */
static int get_seg_coordinate(INP t_rr_type chan_type, INP int x_coord, INP int y_coord){
	int seg = -1;
	
	switch (chan_type){
		case CHANX:
			seg = x_coord;
			break;
		case CHANY:
			seg = y_coord;
			break;
		default:
			vpr_printf(TIO_MESSAGE_ERROR, "get_seg_coordinate: 'chan_type' is not a channel\n");
			exit(1);
			break;
	}

	return seg;
}

/* returns the direction in which a source track at the specified
   switchblock side should be headed. i.e. if we connect from a track on the top
   side of a switchblock in a unidir architecture, this track should be
   going in the -y direction (aka in the direction of 'decreasing' coordinate) . */
static e_direction get_src_direction(e_side from_side, e_directionality directionality){
	e_direction src_dir;

	if (BI_DIRECTIONAL == directionality){
		src_dir = BI_DIRECTION;
	} else if (UNI_DIRECTIONAL == directionality){
		if (TOP == from_side || RIGHT == from_side){
			/* in direction of decreasing coordinate */
			src_dir = DEC_DIRECTION;
		} else if (LEFT == from_side || BOTTOM == from_side){
			/* in direction of increasing coordinate */
			src_dir = INC_DIRECTION;
		} else {
			vpr_printf(TIO_MESSAGE_ERROR, "get_src_direction(): unkown switchblock side %d\n", from_side);
			exit(1);
		}
	} else {
		vpr_printf(TIO_MESSAGE_ERROR, "get_src_direction(): unknown directionality %d\n", directionality);
		exit(1);
	}

	return src_dir;
}

/* returns the direction in which a destination track at the specified
   switchblock side should be headed. i.e. if we connect to a track on the top
   side of a switchblock in a unidir architecture, this track should be
   going in the +y direction (aka in the direction of 'increasing' coordinate) . */
static e_direction get_dest_direction(e_side to_side, e_directionality directionality){
	e_direction dest_dir;

	if (BI_DIRECTIONAL == directionality){
		dest_dir = BI_DIRECTION;
	} else if (UNI_DIRECTIONAL == directionality){
		if (TOP == to_side || RIGHT == to_side){
			/* in direction of increasing coordinate */
			dest_dir = INC_DIRECTION;
		} else if (LEFT == to_side || BOTTOM == to_side){
			/* in direction of decreasing coordinate */
			dest_dir = DEC_DIRECTION;
		} else {
			vpr_printf(TIO_MESSAGE_ERROR, "get_dest_direction(): unkown switchblock side %d\n", to_side);
			exit(1);
		}
	} else {
		vpr_printf(TIO_MESSAGE_ERROR, "get_dest_direction(): unknown directionality %d\n", directionality);
		exit(1);
	}

	return dest_dir;
}

/* returns the group number of the specified track */
static int get_track_group(INP t_seg_details &track_details, int seg_coord){
	
	/* We get track group by comparing the track's seg_coord to the seg_start of the track.
	   The offset between seg_start and seg_coord is the group number */

	int group;
	int seg_start = track_details.seg_start;
	group = seg_coord - seg_start;

	/* the start of this track cannot be *after* the current seg coordinate */
	if (group < 0){
		vpr_printf(TIO_MESSAGE_ERROR, "get_track_group: invalid seg_start, seg_coord combination\n");
		exit(1); 
	}

	return group;
}

/* Finds the index of the first track in the channel specified by 'track_details'
   to have the type name specified by 'type' */
static int find_start_of_track_type(INP t_seg_details *track_details, INP const char *type, 
			INP int nodes_per_chan, INP t_track_type_sizes track_type_sizes){

	string map_accessor;
	int track = 0;
	while( track < nodes_per_chan ){
		/* check type of track */
		if ( strcmp(type, track_details[track].type_name_ptr) == 0 ){
			/* track type found */
			break;
		} else {
			/* move on to next track type */
			map_accessor = track_details[track].type_name_ptr;
			track += track_type_sizes[map_accessor];
		}
	}

	if (track >= nodes_per_chan){
		vpr_printf(TIO_MESSAGE_ERROR, "find_start_of_track_type: track type %s could not be found in the channel\n", type);
		exit(1);
	}

	return track;

} 

/* returns the track index in the channel specified by 'track_details' that corresponds to the 
   first occurence of the specified track group (belonging to type referred to by track_type_start, 
   track_type_size, and track_direction) */
static int find_start_of_track_group(INP t_seg_details *track_details, INP const char *type, 
			INP int track_group, INP int track_seg_coord, INP e_direction track_direction, 
			INP int track_type_start, INP int nodes_per_chan, INP t_track_type_sizes track_type_sizes){
	string map_accessor = type;
	int track_type_size = track_type_sizes[map_accessor];
	int track_type_end = track_type_start + track_type_size - 1;	

	int track;
	for (track = track_type_start; track <= track_type_end; track++){
		e_direction direction = track_details[track].direction;
		int group = get_track_group(track_details[track], track_seg_coord);
		if (group == track_group && direction == track_direction){
			/* found the requested track group */
			break;
		}

		/* if we hit the last track but still haven't found the group... */
		if (track == track_type_end){
			vpr_printf(TIO_MESSAGE_ERROR, "track group %d could not be found in the channel\n", track_group);
			exit(1);
		}
	}

	return track;
}

/* fills the 'tracks_in_group' vector with the indices of the tracks in the 'track_details' channel
   which have the specified 'track_type', 'track_group', and are going in 'track_direction' */
static void get_group_tracks(INP t_seg_details *track_details, INP const char *track_type, INP int track_group,
			INP int track_seg_coord, INP e_direction track_direction, INP int nodes_per_chan, 
			INP t_track_type_sizes track_type_sizes, INOUTP vector<int> *tracks_in_group){
	int group_size;			/* the number of tracks in the channel belonging to specified type->group */
	int type_start, group_start;	/* track indices of where given 'type' and 'group' start respectively */
	int track_type_end;		/* track index where the track type ends */
	int track_length;		/* length of track */
	int dir_mult;			/* multiplier to account for unidir/bidir differences */
	int step_size;			/* spacing between two consecutive tracks of the same group/type */
	
	/* multiplier to account for unidir vs bidir differences */
	if (BI_DIRECTION == track_direction){
		dir_mult = 1;
	} else if (INC_DIRECTION == track_direction || DEC_DIRECTION == track_direction){
		dir_mult = 2;
	} else {
		vpr_printf(TIO_MESSAGE_ERROR, "get_group_size(): unknown track direction %d\n", track_direction);
		exit(1);
	}

	/* get index of track in channel at which the specified type->group starts */
	type_start = find_start_of_track_type(track_details, track_type, nodes_per_chan, track_type_sizes);
	group_start = find_start_of_track_group(track_details, track_type, track_group, 
					track_seg_coord, track_direction, type_start, nodes_per_chan, 
					track_type_sizes);
	
	string map_accessor = track_type;
	track_type_end = type_start + track_type_sizes[map_accessor] - 1;
	track_length = track_details[group_start].length;

	/* get the group size. Consecutive tracks of the same group
	   are separated by 'track_length' tracks for the bidirectional case,
	   and by 2*track_length tracks for the unidirectional case, because for 
	   unidir tracks are grouped in pairs. */
	step_size = track_length * dir_mult;
	group_size = track_type_end - group_start;
	group_size = floor(group_size / step_size) + 1;

	/* and now we set which track indices in the channel correspond to the 
	   desired type, group, and directionality */
	tracks_in_group->reserve(group_size);
	for (int i = 0; i < group_size; i++){
		tracks_in_group->push_back( group_start + i*step_size );
	}

	return;
}


///* checks whether the entry specified by coord exists in the switchblock map sb_conns */
//static bool sb_connection_exists( INP Switchblock_Lookup coord, INP t_sb_connection_map *sb_conns){
//	bool result;
//
//	t_sb_connection_map::const_iterator it = (*sb_conns).find(coord);
//	if ((*sb_conns).end() != it){
//		result = true;
//	} else {
//		result = false;
//	}
//
//	return result;
//}