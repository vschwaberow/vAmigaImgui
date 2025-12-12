import sys
import os

def binary_to_c_header(input_file, output_file, var_name):
    with open(input_file, 'rb') as f:
        data = f.read()

    with open(output_file, 'w') as f:
        f.write('#pragma once\n\n')
        f.write('// Generated from ' + os.path.basename(input_file) + '\n')
        f.write('static const unsigned int ' + var_name + '_size = ' + str(len(data)) + ';\n')
        f.write('static const unsigned char ' + var_name + '_data[] = {\n')
        
        for i, byte in enumerate(data):
            if i % 12 == 0:
                f.write('    ')
            f.write('0x{:02x}, '.format(byte))
            if (i + 1) % 12 == 0:
                f.write('\n')
        
        f.write('\n};\n')

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("Usage: python script.py <input_file> <output_file> <var_name>")
        sys.exit(1)
    
    binary_to_c_header(sys.argv[1], sys.argv[2], sys.argv[3])
