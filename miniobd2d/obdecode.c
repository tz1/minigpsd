#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>

// strings indicating the meaning of the PID, by PID
char *whats[128] = {
    "*","MonStat","Frz","Fuel Sys Stat","Load","Coolant Temp",
    "Bank 1 Fuel Trim, short","Bank 1 Fuel Trim, long", "Bank 2 Fuel Trim, short","Bank 2 Fuel Trim, long",
    "Fuel Press","Manifold Press", "RPM","VehSpd","Timing Advance","Intake Air Temp",
    "Mass Air Flow rate","Throttle Pos","Cmd 2nd Air","O2s", 
    "STFuelTrim O2Volt, % B1S1","STFuelTrim O2Volt, % B1S2","STFuelTrim O2Volt, % B1S3","STFuelTrim O2Volt, % B1S4",
    "STFuelTrim O2Volt, % B2S1","STFuelTrim O2Volt, % B2S2","STFuelTrim O2Volt, % B2S3","STFuelTrim O2Volt, % B2S4",
    "Conform","02s","AuxI","since Engine start",

    "*","Distance with MIL","FuelRail Press (v. ManiVac)","FuelRail Press", 
    "Lambda S1, Volts","Lambda S2, Volts","Lambda S3, Volts","Lambda S4, Volts", "Lambda S5, Volts","Lambda S6, Volts","Lambda S7, Volts","Lambda S8, Volts",
    "Cmd EGR","EGR Err","Cmd Evap Purge","Fuel Level",

    "Warmups since clear","since clear","Evap sys vapor press","Baro", 
    "Lambda S1, Amps","Lambda S2, Amps","Lambda S3, Amps","Lambda S4, Amps", "Lambda S5, Amps","Lambda S6, Amps","Lambda S7, Amps","Lambda S8, Amps",
    "Cat temp B1S1","Cat temp B2S1","Cat temp B1S2","Cat temp B2S2",

    "*","Mon Stat","Module Voltage","Abs Load", "Cmd Equiv Ratio","Rel Throttle Pos","Ambient Air Temp",
    "Abs Throt Pos B", "Abs Throt Pos C","Accel Pedal D","Accel Pedal E","Accel Pedal F", "Cmd Throt Actuator",
    "run time with Mil On","since codes cleared","?",
    "?","?","?","?", "?","?","?","?", "?","?","?","?", "?","?","?","?",
    "*","?","?","?", "?","?","?","?", "?","?","?","?", "?","?","?","?",
    "?","?","?","?", "?","?","?","?", "?","?","?","?", "?","?","?","?",
};

int main()
{
    double dva;
    char *c, buf[80];
    // memimg is a memory mapped array of PIDs updated by miniobd2d.
    // state is the previous value so that changes can be found (and only changes printed out).
    unsigned int *memimg, state[128], val;
    int j, mfd;

    // set initial state to 0xa5 bytes to force a first refresh for valid data
    // miniobd2d does the same so unsupported or no-data pids will not have an initial print
    memset(state, 0xa5, sizeof(state));

    // this file is created and updated by miniobd2.
    // it consists of readings for each supported PID (based on 0x00, then maybe 0x20, 0x40, etc.).
    // The PID data read in the form of unsigned integers in machine endianness, extended to 4
    // bytes (e.g. 2 byte PID data is in the two least significant bytes with the upper two bytes
    // zero.  The data is raw and unsigned so values which might be better interpreted as negative
    // are left for the conversion functions in the switch statement below
    mfd = open("/tmp/obd2state", O_RDWR);
    if (mfd < 0)
        return -1;
    // mmap the small file to create an array of 128 unsigned long raw PID data.
    memimg = mmap(0, sizeof(state), PROT_READ, MAP_SHARED, mfd, 0);

    for (;;)
        // should sleep or yield or something here since PIDs have a finite refresh speed
        for (j = 0; j < 128; j++) {
            // read PID[j], and see if it is different from teh last loop
            val = memimg[j];
            if (val == state[j])
                continue;
            // remember for change detect
            state[j] = val;
            // convert to floating (for most of the following calculations)
            dva = (double) val;

            // compose string of engineering units (value symbol) by PID
            switch (j) {
                // bitmapped or unity

            case 0x0b:
            case 0x33:
                sprintf(buf, "%.3f kPa", dva);
                break;
            case 0x0d:         // speed
                sprintf(buf, "%.3f kPH", dva);
                break;
                // percent
            case 0x04:         // load
            case 0x11:         // throttle pos
            case 0x2c:         // egr
            case 0x2e:         // evap purge
            case 0x2f:         // fuel level
            case 0x45:
            case 0x47:
            case 0x48:
            case 0x49:
            case 0x4a:
            case 0x4b:
            case 0x4c:
            case 0x52:         //ethanol
                dva = dva * 100.0 / 255.0;
                sprintf(buf, "%.3f %%", dva);
                break;
            case 0x1f:         // run time since start
                dva = dva * 100.0 / 255.0;
                sprintf(buf, "%.3f s", dva);
                break;
            case 0x4d:
            case 0x4e:
                dva = dva * 100.0 / 255.0;
                sprintf(buf, "%.3f min", dva);
                break;
            case 0x21:         // distance with MIL
            case 0x31:         // distance since cleared
                dva = dva * 100.0 / 255.0;
                sprintf(buf, "%.3f k", dva);
                break;

                // lorange temp
            case 0x05:         //coolant
            case 0x0f:         //intake
            case 0x46:         //ambient
                dva = dva - 40.0;
                sprintf(buf, "%.3f Deg C", dva);
                break;
                // -100 to 100
            case 0x06:
            case 0x07:
            case 0x08:
            case 0x09:
                dva = (dva - 128.0) * 100.0 / 128.0;
                sprintf(buf, "%.3f %%", dva);
                break;
            case 0x0a:
                dva = dva * 3.0;
                sprintf(buf, "%.3f kPa", dva);
                break;
            case 0x0c:         // RPM
                dva = dva / 4.0;
                sprintf(buf, "%.3f RPM", dva);
                break;
            case 0x0e:
                dva = (dva - 128.0) / 2.0;
                sprintf(buf, "%.3f Deg", dva);
                break;
            case 0x10:         // maf rate g/s
                dva = dva / 100.0;
                sprintf(buf, "%.3f g/s", dva);
                break;
            case 0x14:         // short term fuel trim
            case 0x15:
            case 0x16:
            case 0x17:
            case 0x18:
            case 0x19:
            case 0x1a:
            case 0x1b:
                dva = (double) (val >> 8) / 200.0;
                sprintf(buf, "%.3f V", dva);
                c = buf;
                c += strlen(buf);
                dva = (double) (val & 0xff) * 100.0 / 128.0 - 1.0;
                sprintf(buf, " %.3f %%", dva);
                // split A / 200 
                //B:      dva =  dva * 100 / 128 - 1.0;
                break;
            case 0x24:         // equidvaence ratio volt
            case 0x25:
            case 0x26:
            case 0x27:
            case 0x28:
            case 0x29:
            case 0x2a:
            case 0x2b:
                sprintf(buf, "Implemented %X", val);
                // split AB / 32768, CD / 8192
                break;
            case 0x2d:         // egr error * 1000
                dva = dva * 100.0 / 128.0 - 100.0;
                sprintf(buf, "%.3f %%", dva);
                break;
            case 0x32:         //evap vap press, Pa * 1000
                dva = dva / 4.0 - 8192.0;
                sprintf(buf, "%.3f Pa", dva);
                break;
            case 0x34:         // equidvaence ratio current
            case 0x35:
            case 0x36:
            case 0x37:
            case 0x38:
            case 0x39:
            case 0x3a:
            case 0x3b:
                // split
                sprintf(buf, "Not Implemented %X", val);
                break;
                // high temp
            case 0x3c:
            case 0x3d:
            case 0x3e:
            case 0x3f:
                dva = dva / 10.0 - 40.0;
                sprintf(buf, "%.3f Deg C", dva);
                break;
            default:
                sprintf(buf, "Unsupported: %X", val);
            }
            // print PID, value and units, and what the PID represents
            printf("%02X %s %s\n", j, buf, whats[j]);
            // force a printout
            fflush(stdout);

        }

}
