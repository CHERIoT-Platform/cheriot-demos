// FFI Imports
// Each function imported from the host environment needs to be assigned to a
// global like this and identified by a constant that the resolver in the C/C++
// code will understand.
// These constants are defined in the `Exports` enumeration.


/**
 * Log function, writes all arguments to the UART.
 */
export const print = vmImport(1);

/*******************************************************************************
 * CHERI functions give access to a virtual machine for writing exploits.  This
 * allows 8 virtual registers to be accessed as source and destination operands
 * to any of the registers.
 *
 * The constants CSP, PCC, and CGP can also be used to access the values in
 * these physical registers directly, but only as source operands.
 ******************************************************************************/

export const CSP = 8
export const CGP = 9
export const PCC = 10

/**
 * register_move(destination, source)
 *
 * Move a value between two registers.  
 */
export const register_move = vmImport(2);

/**
 * load_capability(destination, source, offset)
 *
 * Loads a capability from the specified offset to the source capability
 * register, into the destination register.
 */
export const load_capability = vmImport(3);

/**
 * load_int(source, offset)
 *
 * Loads and returns an integer from memory, from the specified offset to the
 * source register.
 */
export const load_int = vmImport(4);

/**
 * store(source, destination, offset)
 *
 * Stores a capability from a source register to the specified offset relative
 * to the destination register.
 */
export const store = vmImport(5);

/**
 * get_address(source)
 *
 * Returns the address for the specified source register (capability).
 */
export const get_address = vmImport(6);

/**
 * set_address(source, address)
 *
 * Sets the address for the specified register (capability).
 */
export const set_address = vmImport(7);

/**
 * get_base(source)
 *
 * Returns the base address for the specified source register (capability).
 */
export const get_base = vmImport(8);

/**
 * get_length(source)
 *
 * Returns the length for the specified source register (capability).
 */
export const get_length = vmImport(9);

/**
 * get_permissions(source)
 *
 * Returns the permissions for the specified source register (capability).
 */
export const get_permissions = vmImport(10);

/**
 * led_on(index).
 *
 * Turns on the LED at the specified index.
 */
export const led_on = vmImport(11);

/**
 * led_off(index).
 *
 * Turns off the LED at the specified index.
 */
export const led_off = vmImport(12);

/*******************************************************************************
 * The button / switch APIs below work only on the Arty A7.
 ******************************************************************************/

/**
 * read_button(index).
 *
 * Reads the value of the button at the specified index.
 */
export const read_button = vmImport(13);

/**
 * read_switch(index).
 *
 * Reads the value of the switch at the specified index.
 */
export const read_switch = vmImport(14);

/**
 * read_buttons().
 *
 * Reads the value of all of the buttons, returning a 4-bit value indicating
 * the states of all of them.
 */
export const read_buttons = vmImport(15);

/**
 * read_switches().
 *
 * Reads the value of all of the switches, returning a 4-bit value indicating
 * the states of all of them.
 */
export const read_switches = vmImport(16);


/**
 * led_set(index, state).
 *
 * Turns the LED at the specified index on or off depending on whether state is
 * true or false.
 */
export const led_set = vmImport(17);

/**
 * Access smart meter information
 */
export const read_from_snapshot = vmImport(18);

export const DATA_SENSOR_TIMESTAMP = 1;
export const DATA_SENSOR_SAMPLE = 2;
export const DATA_GRID_OUTAGE_START = 3;
export const DATA_GRID_OUTAGE_DURATION = 4;
export const DATA_GRID_REQUEST_START_TIME = 5;
export const DATA_GRID_REQUEST_DURATION = 6;
export const DATA_GRID_REQUEST_SEVERITY = 7;
export const DATA_PROVIDER_SCHEDULE_TIMESTAMP_DAY = 8;
export const DATA_PROVIDER_SCHEDULE_RATE = 9;
export const DATA_PROVIDER_VARIANCE_TIMESTAMP_BASE = 10;
export const DATA_PROVIDER_VARIANCE_START = 11;
export const DATA_PROVIDER_VARIANCE_DURATION = 12;
export const DATA_PROVIDER_VARIANCE_RATE = 13;
