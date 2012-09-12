/************************* INCLUDES *********************************/
#include <assert.h>
#include <string.h>

#include "power_cmos_tech.h"
#include "power.h"
#include "power_util.h"
#include "ezxml.h"
#include "util.h"
#include "read_xml_util.h"

/************************* GLOBALS **********************************/
static t_transistor_inf * g_transistor_last_searched;
static t_power_buffer_strength_inf * g_buffer_strength_last_searched;

/************************* FUNCTION DECLARATIONS ********************/
static void process_transistor_info(ezxml_t parent);
static void process_multiplexer_info(ezxml_t parent);
static void process_nmos_leakages(ezxml_t parent);
static int power_compare_transistor_size(const void * key_void,
		const void * elem_void);
static int power_compare_voltage_pair(const void * key_void,
		const void * elem_void);
static int power_compare_leakage_pair(const void * key_void,
		const void * elem_void);
static void process_buffer_sc(ezxml_t parent);
static int power_compare_buffer_strength(const void * key_void,
		const void * elem_void);
static int power_compare_buffer_sc_levr(const void * key_void,
		const void * elem_void);

/************************* FUNCTION DEFINITIONS *********************/
void power_read_cmos_tech_behavior(void) {
	ezxml_t cur, child, prev;
	const char * prop;
	char msg[BUFSIZE];

	if (!file_exists(g_power_opts->cmos_tech_behavior_file)) {
		sprintf(msg,
				"The CMOS technology behavior file ('%s') does not exist.  No power information will be calculated.",
				g_power_opts->cmos_tech_behavior_file);
		power_log_msg(POWER_LOG_ERROR, msg);
		g_power_tech->NMOS_inf.num_size_entries = 0;
		g_power_tech->NMOS_inf.long_trans_inf = NULL;
		g_power_tech->NMOS_inf.size_inf = NULL;

		g_power_tech->PMOS_inf.num_size_entries = 0;
		g_power_tech->PMOS_inf.long_trans_inf = NULL;
		g_power_tech->PMOS_inf.size_inf = NULL;

		g_power_tech->Vdd = 0.;
		g_power_tech->temperature = 85;
		g_power_tech->PN_ratio = 1.;
		return;
	}
	cur = ezxml_parse_file(g_power_opts->cmos_tech_behavior_file);

	prop = FindProperty(cur, "file", TRUE);
	ezxml_set_attr(cur, "file", NULL );

	prop = FindProperty(cur, "size", TRUE);
	g_power_tech->tech_size = atof(prop);
	ezxml_set_attr(cur, "size", NULL );

	child = FindElement(cur, "operating_point", TRUE);
	g_power_tech->temperature = GetFloatProperty(child, "temperature", TRUE, 0);
	g_power_tech->Vdd = GetFloatProperty(child, "Vdd", TRUE, 0);
	FreeNode(child);

	child = FindElement(cur, "p_to_n", TRUE);
	g_power_tech->PN_ratio = GetFloatProperty(child, "ratio", TRUE, 0);
	FreeNode(child);

	/* Transistor Information */
	child = FindFirstElement(cur, "transistor", TRUE);
	process_transistor_info(child);

	prev = child;
	child = child->next;
	FreeNode(prev);

	process_transistor_info(child);
	FreeNode(child);

	/* Multiplexer Voltage Information */
	child = FindElement(cur, "multiplexers", TRUE);
	process_multiplexer_info(child);
	FreeNode(child);

	/* Vds Leakage Information */
	child = FindElement(cur, "nmos_leakages", TRUE);
	process_nmos_leakages(child);
	FreeNode(child);

	/* Buffer SC Info */
	child = FindElement(cur, "buffer_sc", TRUE);
	process_buffer_sc(child);
	FreeNode(child);

	FreeNode(cur);
}

struct s_power_buffer_sc {

};

static void process_buffer_sc(ezxml_t parent) {
	ezxml_t child, prev, gc, ggc;
	int i, j, k;
	int num_buffer_sizes;

	num_buffer_sizes = CountChildren(parent, "stages", 1);
	g_power_tech->max_buffer_size = num_buffer_sizes + 1; /* buffer size starts at 1, not 0 */
	g_power_tech->buffer_size_inf = my_calloc(num_buffer_sizes,
			sizeof(t_power_buffer_size_inf));

	child = FindFirstElement(parent, "stages", TRUE);
	i = 1;
	while (child) {
		t_power_buffer_size_inf * size_inf = &g_power_tech->buffer_size_inf[i];

		GetIntProperty(child, "num_stages", TRUE, 1);
		size_inf->num_strengths = CountChildren(child, "strength", 1);
		size_inf->strength_inf = my_calloc(size_inf->num_strengths,
				sizeof(t_power_buffer_strength_inf));

		gc = FindFirstElement(child, "strength", TRUE);
		j = 0;
		while (gc) {
			t_power_buffer_strength_inf * strength_inf =
					&size_inf->strength_inf[j];

			strength_inf->stage_gain = GetFloatProperty(gc, "gain", TRUE, 0.0);
			strength_inf->sc_no_levr = GetFloatProperty(gc, "sc_nolevr", TRUE,
					0.0);

			strength_inf->num_levr_entries = CountChildren(gc, "input_cap", 1);
			strength_inf->sc_levr_inf = my_calloc(
					strength_inf->num_levr_entries,
					sizeof(t_power_buffer_sc_levr_inf));

			ggc = FindFirstElement(gc, "input_cap", TRUE);
			k = 0;
			while (ggc) {
				t_power_buffer_sc_levr_inf * levr_inf =
						&strength_inf->sc_levr_inf[k];

				levr_inf->mux_size = GetIntProperty(ggc, "mux_size", TRUE, 0);
				levr_inf->sc_levr = GetFloatProperty(ggc, "sc_levr", TRUE, 0.0);

				prev = ggc;
				ggc = ggc->next;
				FreeNode(prev);
				k++;
			}

			prev = gc;
			gc = gc->next;
			FreeNode(prev);
			j++;
		}

		prev = child;
		child = child->next;
		FreeNode(prev);
		i++;
	}
}

static void process_nmos_leakages(ezxml_t parent) {
	ezxml_t child, prev;
	int num_leakage_pairs;
	int i;

	num_leakage_pairs = CountChildren(parent, "nmos_leakage", 1);
	g_power_tech->num_leakage_pairs = num_leakage_pairs;
	g_power_tech->leakage_pairs = my_calloc(num_leakage_pairs,
			sizeof(t_power_nmos_leakage_pair));

	child = FindFirstElement(parent, "nmos_leakage", TRUE);
	i = 0;
	while (child) {
		g_power_tech->leakage_pairs[i].v_ds = GetFloatProperty(child, "Vds",
				TRUE, 0.0);
		g_power_tech->leakage_pairs[i].i_ds = GetFloatProperty(child, "Ids",
				TRUE, 0.0);

		prev = child;
		child = child->next;
		FreeNode(prev);
		i++;
	}

}

static void process_multiplexer_info(ezxml_t parent) {
	ezxml_t child, prev, gc;
	int num_mux_sizes;
	int i, j;

	num_mux_sizes = CountChildren(parent, "multiplexer", 1);

	/* Add entries for 0 and 1, for convenience, although
	 * they will never be used
	 */
	g_power_tech->max_mux_sl_size = 1 + num_mux_sizes;
	g_power_tech->mux_voltage_inf = my_calloc(g_power_tech->max_mux_sl_size + 1,
			sizeof(t_power_mux_volt_inf));

	child = FindFirstElement(parent, "multiplexer", TRUE);
	i = 2;
	while (child) {
		int num_voltages;

		assert(i == GetFloatProperty(child, "size", TRUE, 0));

		num_voltages = CountChildren(child, "voltages", 1);

		g_power_tech->mux_voltage_inf[i].num_voltage_pairs = num_voltages;
		g_power_tech->mux_voltage_inf[i].mux_voltage_pairs = my_calloc(
				num_voltages, sizeof(t_power_mux_volt_pair));

		gc = FindFirstElement(child, "voltages", TRUE);
		j = 0;
		while (gc) {
			g_power_tech->mux_voltage_inf[i].mux_voltage_pairs[j].v_in =
					GetFloatProperty(gc, "in", TRUE, 0.0);
			g_power_tech->mux_voltage_inf[i].mux_voltage_pairs[j].v_out_min =
					GetFloatProperty(gc, "out_min", TRUE, 0.0);
			g_power_tech->mux_voltage_inf[i].mux_voltage_pairs[j].v_out_max =
					GetFloatProperty(gc, "out_max", TRUE, 0.0);

			prev = gc;
			gc = gc->next;
			FreeNode(prev);
			j++;
		}

		prev = child;
		child = child->next;
		FreeNode(prev);
		i++;
	}
}

static void process_transistor_info(ezxml_t parent) {
	t_transistor_inf * trans_inf;
	const char * prop;
	ezxml_t child, prev, grandchild;
	int i;

	prop = FindProperty(parent, "type", TRUE);
	if (strcmp(prop, "nmos") == 0) {
		trans_inf = &g_power_tech->NMOS_inf;
	} else if (strcmp(prop, "pmos") == 0) {
		trans_inf = &g_power_tech->PMOS_inf;
	} else {
		assert(0);
	}
	ezxml_set_attr(parent, "type", NULL );

	/*trans_inf->Vth = GetFloatProperty(parent, "Vth", TRUE, 0.);*/

	trans_inf->long_trans_inf = my_malloc(sizeof(t_transistor_size_inf));

	child = FindElement(parent, "long_size", TRUE);
	assert(GetIntProperty(child, "L", TRUE, 0) == 2);
	trans_inf->long_trans_inf->size = GetFloatProperty(child, "W", TRUE, 0);

	grandchild = FindElement(child, "leakage_current", TRUE);
	trans_inf->long_trans_inf->leakage_subthreshold = GetFloatProperty(
			grandchild, "subthreshold", TRUE, 0);
	FreeNode(grandchild);

	grandchild = FindElement(child, "capacitance", TRUE);
	trans_inf->long_trans_inf->C_gate_cmos = GetFloatProperty(grandchild,
			"gate_cmos", TRUE, 0);
	/*trans_inf->long_trans_inf->C_source_cmos = GetFloatProperty(grandchild,
	 "source_cmos", TRUE, 0);*/
	trans_inf->long_trans_inf->C_drain_cmos = GetFloatProperty(grandchild,
			"drain_cmos", TRUE, 0);
	trans_inf->long_trans_inf->C_gate_pass = GetFloatProperty(grandchild,
			"gate_pass", TRUE, 0);
	trans_inf->long_trans_inf->C_source_pass = GetFloatProperty(grandchild,
			"source_pass", TRUE, 0);
	trans_inf->long_trans_inf->C_drain_pass = GetFloatProperty(grandchild,
			"drain_pass", TRUE, 0);
	FreeNode(grandchild);

	trans_inf->num_size_entries = CountChildren(parent, "size", 1);
	trans_inf->size_inf = my_calloc(trans_inf->num_size_entries,
			sizeof(t_transistor_size_inf));
	FreeNode(child);

	child = FindFirstElement(parent, "size", TRUE);
	i = 0;
	while (child) {
		assert(GetIntProperty(child, "L", TRUE, 0) == 1);

		trans_inf->size_inf[i].size = GetFloatProperty(child, "W", TRUE, 0);

		grandchild = FindElement(child, "leakage_current", TRUE);
		trans_inf->size_inf[i].leakage_subthreshold = GetFloatProperty(
				grandchild, "subthreshold", TRUE, 0);
		trans_inf->size_inf[i].leakage_gate = GetFloatProperty(grandchild,
				"gate", TRUE, 0);
		FreeNode(grandchild);

		grandchild = FindElement(child, "capacitance", TRUE);
		trans_inf->size_inf[i].C_gate_cmos = GetFloatProperty(grandchild,
				"gate_cmos", TRUE, 0);
		/*trans_inf->size_inf[i].C_source_cmos = GetFloatProperty(grandchild,
		 "source_cmos", TRUE, 0); */
		trans_inf->size_inf[i].C_drain_cmos = GetFloatProperty(grandchild,
				"drain_cmos", TRUE, 0);
		trans_inf->size_inf[i].C_gate_pass = GetFloatProperty(grandchild,
				"gate_pass", TRUE, 0);
		trans_inf->size_inf[i].C_source_pass = GetFloatProperty(grandchild,
				"source_pass", TRUE, 0);
		trans_inf->size_inf[i].C_drain_pass = GetFloatProperty(grandchild,
				"drain_pass", TRUE, 0);
		FreeNode(grandchild);

		prev = child;
		child = child->next;
		FreeNode(prev);
		i++;
	}
}

boolean power_find_transistor_info(t_transistor_size_inf ** lower,
		t_transistor_size_inf ** upper, e_tx_type type, float size) {
	char msg[1024];
	t_transistor_size_inf key;
	t_transistor_size_inf * found;
	t_transistor_inf * trans_info;
	float min_size, max_size;
	boolean error = FALSE;

	key.size = size;

	if (type == NMOS) {
		trans_info = &g_power_tech->NMOS_inf;
	} else if (type == PMOS) {
		trans_info = &g_power_tech->PMOS_inf;
	} else {
		assert(0);
	}

	/* No transistor data exists */
	if (trans_info->size_inf == NULL ) {
		power_log_msg(POWER_LOG_ERROR,
				"No transistor information exists.  Cannot determine transistor properties.");
		error = TRUE;
		return error;
	}

	g_transistor_last_searched = trans_info;
	min_size = trans_info->size_inf[0].size;
	max_size = trans_info->size_inf[trans_info->num_size_entries - 1].size;

	found = bsearch(&key, trans_info->size_inf, trans_info->num_size_entries,
			sizeof(t_transistor_size_inf), power_compare_transistor_size);
	assert(found);

	/* Too small */
	if (size < min_size) {
		assert(found == &trans_info->size_inf[0]);
		sprintf(msg,
				"Using %s transistor of size '%f', which is smaller than the smallest modeled transistor (%f) in the technology behavior file.",
				transistor_type_name(type), size, min_size);
		power_log_msg(POWER_LOG_WARNING, msg);
		*lower = NULL;
		*upper = found;
	} else if (size > max_size) {
		assert(
				found == &trans_info->size_inf[trans_info->num_size_entries - 1]);
		sprintf(msg,
				"Using %s transistor of size '%f', which is larger than the largest modeled transistor (%f) in the technology behavior file.",
				transistor_type_name(type), size, max_size);
		power_log_msg(POWER_LOG_WARNING, msg);
		*lower = found;
		*upper = NULL;
	} else {
		*lower = found;
		*upper = found + 1;
	}

	return error;
}

void power_find_nmos_leakage(t_power_nmos_leakage_pair ** lower,
		t_power_nmos_leakage_pair ** upper, float v_ds) {
	t_power_nmos_leakage_pair key;
	t_power_nmos_leakage_pair * found;

	key.v_ds = v_ds;

	assert(v_ds > (g_power_tech->Vdd / 2) && v_ds <= g_power_tech->Vdd);

	found = bsearch(&key, g_power_tech->leakage_pairs,
			g_power_tech->num_leakage_pairs, sizeof(t_power_nmos_leakage_pair),
			power_compare_leakage_pair);
	assert(found);

	*lower = found;
	*upper = found + 1;
}

void power_find_buffer_strength_inf(t_power_buffer_strength_inf ** lower,
		t_power_buffer_strength_inf ** upper,
		t_power_buffer_size_inf * size_inf, float stage_gain) {
	t_power_buffer_strength_inf key;
	t_power_buffer_strength_inf * found;

	float min_size;
	float max_size;

	min_size = size_inf->strength_inf[0].stage_gain;
	max_size = size_inf->strength_inf[size_inf->num_strengths - 1].stage_gain;

	assert(stage_gain >= min_size && stage_gain <= max_size);

	key.stage_gain = stage_gain;

	found = bsearch(&key, size_inf->strength_inf,
			size_inf->num_strengths,
			sizeof(t_power_buffer_strength_inf), power_compare_buffer_strength);

	if (stage_gain == max_size) {
		*lower = found;
		*upper = NULL;
	} else {
		*lower = found;
		*upper = found + 1;
	}
}

void power_find_buffer_sc_levr(t_power_buffer_sc_levr_inf ** lower,
		t_power_buffer_sc_levr_inf ** upper,
		t_power_buffer_strength_inf * buffer_strength, int input_mux_size) {
	t_power_buffer_sc_levr_inf key;
	t_power_buffer_sc_levr_inf * found;
	char msg[1024];
	int max_size;

	assert(input_mux_size >= 1);

	key.mux_size = input_mux_size;

	g_buffer_strength_last_searched = buffer_strength;
	found = bsearch(&key, buffer_strength->sc_levr_inf, buffer_strength->num_levr_entries,
			sizeof(t_power_buffer_sc_levr_inf), power_compare_buffer_sc_levr);

	max_size = buffer_strength->sc_levr_inf[buffer_strength->num_levr_entries - 1].mux_size;
	if (input_mux_size > max_size) {
		assert(
				found == &buffer_strength->sc_levr_inf[buffer_strength->num_levr_entries-1]);
		sprintf(msg,
				"Using buffer driven by mux of size '%d', which is larger than the largest modeled size (%d) in the technology behavior file.",
				input_mux_size, max_size);
		power_log_msg(POWER_LOG_WARNING, msg);
		*lower = found;
		*upper = NULL;
	} else {
		*lower = found;
		*upper = found + 1;
	}
}

static int power_compare_leakage_pair(const void * key_void,
		const void * elem_void) {
	const t_power_nmos_leakage_pair * key = key_void;
	const t_power_nmos_leakage_pair * elem = elem_void;
	const t_power_nmos_leakage_pair * next = elem + 1;

	/* Check for exact match to Vdd (upper end) */
	if (key->v_ds == elem->v_ds) {
		return 0;
	} else if (key->v_ds < elem->v_ds) {
		return -1;
	} else if (key->v_ds > next->v_ds) {
		return 1;
	} else {
		return 0;
	}
}

void power_find_mux_volt_inf(t_power_mux_volt_pair ** lower,
		t_power_mux_volt_pair ** upper, t_power_mux_volt_inf * volt_inf,
		float v_in) {
	t_power_mux_volt_pair key;
	t_power_mux_volt_pair * found;

	key.v_in = v_in;

	assert(v_in > (g_power_tech->Vdd / 2) && v_in <= g_power_tech->Vdd);

	found = bsearch(&key, volt_inf->mux_voltage_pairs,
			volt_inf->num_voltage_pairs, sizeof(t_power_mux_volt_pair),
			power_compare_voltage_pair);
	assert(found);

	*lower = found;
	*upper = found + 1;
}

static int power_compare_buffer_sc_levr(const void * key_void,
		const void * elem_void) {
	const t_power_buffer_sc_levr_inf * key = key_void;
	const t_power_buffer_sc_levr_inf * elem = elem_void;
	const t_power_buffer_sc_levr_inf * next;

	/* Compare against last? */
	if (elem
			== &g_buffer_strength_last_searched->sc_levr_inf[g_buffer_strength_last_searched->num_levr_entries
					- 1]) {
		if (key->mux_size >= elem->mux_size) {
			return 0;
		} else {
			return -1;
		}
	}

	/* Compare against first? */
	if (elem == &g_buffer_strength_last_searched->sc_levr_inf[0]) {
		if (key->mux_size < elem->mux_size) {
			return 0;
		}
	}

	/* Check if the key is between elem and the next element */
	next = elem + 1;
	if (key->mux_size > next->mux_size) {
		return 1;
	} else if (key->mux_size < elem->mux_size) {
		return -1;
	} else {
		return 0;
	}
}

static int power_compare_buffer_strength(const void * key_void,
		const void * elem_void) {
	const t_power_buffer_strength_inf * key = key_void;
	const t_power_buffer_strength_inf * elem = elem_void;
	const t_power_buffer_strength_inf * next = elem + 1;

	/* Check for exact match */
	if (key->stage_gain == elem->stage_gain) {
		return 0;
	} else if (key->stage_gain < elem->stage_gain) {
		return -1;
	} else if (key->stage_gain > next->stage_gain) {
		return 1;
	} else {
		return 0;
	}
}

static int power_compare_transistor_size(const void * key_void,
		const void * elem_void) {
	const t_transistor_size_inf * key = key_void;
	const t_transistor_size_inf * elem = elem_void;
	const t_transistor_size_inf * next;

	/* Check if we are comparing against the last element */
	if (elem
			== &g_transistor_last_searched->size_inf[g_transistor_last_searched->num_size_entries
					- 1]) {
		/* Match if the desired value is larger than the largest item in the list */
		if (key->size >= elem->size) {
			return 0;
		} else {
			return -1;
		}
	}

	/* Check if we are comparing against the first element */
	if (elem == &g_transistor_last_searched->size_inf[0]) {
		/* Match the smallest if it is smaller than the smallest */
		if (key->size < elem->size) {
			return 0;
		}
	}

	/* Check if the key is between elem and the next element */
	next = elem + 1;
	if (key->size > next->size) {
		return 1;
	} else if (key->size < elem->size) {
		return -1;
	} else {
		return 0;
	}

}

static int power_compare_voltage_pair(const void * key_void,
		const void * elem_void) {
	const t_power_mux_volt_pair * key = key_void;
	const t_power_mux_volt_pair * elem = elem_void;
	const t_power_mux_volt_pair * next = elem + 1;

	/* Check for exact match to Vdd (upper end) */
	if (key->v_in == elem->v_in) {
		return 0;
	} else if (key->v_in < elem->v_in) {
		return -1;
	} else if (key->v_in > next->v_in) {
		return 1;
	} else {
		return 0;
	}
}

