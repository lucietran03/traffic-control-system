/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Team QNX
    Member: Tran Dong Nghi - s3914633
			Le Hung - s4061665
			Hoang Minh Thang - s3999925
    Assessment: 2 - Project Implementation 
    Due date: 18/09/2026
*/

#include <stdio.h>

#include "c_hmi.h"

// Maps controller IDs to short human-readable terminal display names.
static const char *controller_name(controller_id_t id)
{
    switch (id) {
    case CTRL_L1:  return "L1";
    case CTRL_L2:  return "L2";
    case CTRL_L3:  return "L3";
    case CTRL_L4:  return "L4";
    case CTRL_L5:  return "L5";
    case CTRL_L6:  return "L6";
    case CTRL_RL1: return "RL1";
    case CTRL_RL2: return "RL2";
    case CTRL_RL3: return "RL3";
    default:       return "?";
    }
}

static const char *role_name(controller_role_t role)
{
    switch (role) {
    case ROLE_CENTRAL:      return "CENTRAL";
    case ROLE_INTERSECTION: return "INTERSECTION";
    case ROLE_RAILWAY:      return "RAILWAY";
    default:                return "?";
    }
}

void c_hmi_render(const c_mode_eng_t *eng)
{
    int i;

    printf("---- C1 network status ----\n");
    printf("%-5s %-13s %-9s %-6s %-16s %-12s %-8s %-9s %-8s %-12s\n",
           "ID", "ROLE", "MODE", "PHASE", "CROSSING_STATE", "SUPERVISORY", "FAULTS", "SENSOR", "OVERRIDE", "AVAILABILITY");

    for (i = 0; i < 9; i++) {
        const c_controller_view_t *c = &eng->controllers[i];
        char phase_buf[8];
        char crossing_buf[8];
        char sensor_buf[10];

        if (c->role == ROLE_INTERSECTION) {
            snprintf(phase_buf, sizeof(phase_buf), "%u", (unsigned)c->last_reported_signal_phase);
            // Extracts sensor status, which is exclusively meaningful for intersection controllers.
            snprintf(sensor_buf, sizeof(sensor_buf), "%#x", (unsigned)c->last_reported_sensor_status);
        } else {
            snprintf(phase_buf, sizeof(phase_buf), "-");
            snprintf(sensor_buf, sizeof(sensor_buf), "-");
        }

        if (c->role == ROLE_RAILWAY) {
            snprintf(crossing_buf, sizeof(crossing_buf), "%u", (unsigned)c->last_reported_crossing_state);
        } else {
            snprintf(crossing_buf, sizeof(crossing_buf), "-");
        }

        printf("%-5s %-13s %-9u %-6s %-16s %-12u %#-8x %-9s %-8u %-12s\n",
               controller_name(c->id),
               role_name(c->role),
               (unsigned)c->last_reported_mode,
               phase_buf,
               crossing_buf,
               (unsigned)c->last_reported_supervisory_state,
               (unsigned)c->last_reported_faults,
               sensor_buf,
               (unsigned)c->last_reported_override_active,
               c->marked_unavailable ? "UNAVAILABLE" : "AVAILABLE");
    }
    printf("----------------------------\n");
    fflush(stdout);
}