#ifndef KBMATRIX_H
#define KBMATRIX_H

#ifdef KBMATRIX

#define KB_ROWS_IN {10, 11, 12, 13}    //input
#define KB_COLUMNS_OUT {14, 15, 16, 17} //output
#define KB_DEBOUNCE_CYCLE 10        // only report the new state when the inputs are stabel in the provided servo thread cycles, 10 cycles = 0.01s
#define KB_PICO 1                   // use PICO when 1, or use Raspberry gpio when 0

// you need to define rows * columns key names, the hal driver generate the hal pins with the provided names
static char *key_names[] = {"softkey-00",  // column 0 - row 0
                            "softkey-01",  // column 0 - row 1
                            "softkey-02",  // column 0 - row 2
                            "softkey-03",  // column 0 - row 3
                            "softkey-04",  // column 1 - row 0
                            "softkey-05",  // column 1 - row 1
                            "softkey-06",  // column 1 - row 2
                            "softkey-07",  // column 1 - row 3
                            "softkey-08",  // column 2 - row 0
                            "softkey-09",  // column 2 - row 1
                            "softkey-10",  // column 2 - row 2
                            "softkey-11",  // column 2 - row 3
                            "softkey-12",  // column 3 - row 0
                            "softkey-13",  // column 3 - row 1
                            "softkey-14",  // column 3 - row 2
                            "softkey-15"}; // column 3 - row 3

#endif // KBMATRIX
#endif // KBMATRIX_H