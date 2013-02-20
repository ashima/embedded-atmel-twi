import sys
import struct
from functools import reduce
import operator

""" Can be a single value or an array (like x,y,z of a gyro)
"""
class BinaryField:
    def __init__(self, name, type, count):
        self.name = name
        self.type = type
        self.count = count

""" Used by struct.unpack, as well as for default string formatting
"""
class BinaryFieldType:
    def __init__(self, name, size, struct_format, display_format):
        self.name = name
        self.size = size
        self.struct_format = struct_format
        self.display_format = display_format

""" Use to decode binary data prefixed with zero or more start bytes, suffixed by zero or one parity
    bytes.
"""
class BinaryRecord:
    # this allows us to dynamically add properties
    class Record:
        pass
    
    default_field_types = {t.name: t for t in [
        BinaryFieldType("char",  1, "c", '%s'),
        BinaryFieldType("uchar", 1, "B", '%d'),
        BinaryFieldType("byte",  1, "B", '%d'),
        BinaryFieldType("int",   2, "h", '%d'),
        BinaryFieldType("uint",  2, "H", '%d'),
        BinaryFieldType("long",  4, "i", '%d'),
        BinaryFieldType("ulong", 4, "I", '%d'),
        BinaryFieldType("float", 4, "f", '%.2f')
        ]}
        
    _parity_name = 'parity'
    
    """ binary_fields: list of BinaryField instances (NOT a dict; we care about ordering!)
        field_types: list of zero or more BinaryFieldType instances (to, for example, override the
                     default display format)
        endianness: < for little-endian (all 8-bit AVR); > for big-endian (ATMEL 32-bit)
    """
    def __init__(self, start_bytes, binary_fields, field_types = [], endianness = '<'):
        self.start_bytes = start_bytes
        self.binary_fields = binary_fields
        self.field_types = dict(BinaryRecord.default_field_types) # make a copy
        self.field_types.update({t.name: t for t in field_types})
        
        d = dict()
        for bf in binary_fields:
            if d.get(bf.name) != None:
                raise ValueError("duplicate BinaryField with name = '%s'" % bf.name)
            d[bf.name] = bf
        
        # the parity calculation would need to change from ^ to something else for parity_size > 1
        p = d.get(BinaryRecord._parity_name)
        if p != None:
            if self.field_types[p.type].size != 1 or p.count != 1:
                raise ValueError("if there is a BinaryField with name = '%s', " +
                                 "it must be of BinaryFieldType with size = 1 and have count = 1" %
                                 BinaryRecord._parity_name)
        self.parity_size = self.field_types[p.type].size if p != None else 0
        
        self.data_byte_count = sum(
            [bf.count * self.field_types[bf.type].size for bf in self.binary_fields])
        self.read_struct_format = endianness + ''.join(
            [bf.count * self.field_types[bf.type].struct_format for bf in self.binary_fields])
        self.total_byte_count = self.data_byte_count + len(self.start_bytes)
        self.buffer = bytearray()
        self.start_byte_idx = 0
        self.byte_seeks = 0
        self.parity_fails = 0
        self.parity_wins = 0
    
    """ Converts a record to a delimited string, excluding the parity field (if there is one).
    """
    def to_string(self, rec, delimiter=', '):
        def bf_to_string(bf):
            type = self.field_types[bf.type]
            vals = [rec.__dict__[bf.name]] if bf.count == 1 else rec.__dict__[bf.name]
            return delimiter.join(map(lambda rec: type.display_format % rec, vals))
        def not_parity(bf):
            return bf.name != BinaryRecord._parity_name
        
        return ', '.join([bf_to_string(bf) for bf in self.binary_fields if not_parity(bf)])
    
    """ See function contents.
    """
    def status(self):
        return 'seeks: %d  parity fails: %d  valid: %d' % \
                (self.byte_seeks, self.parity_fails, self.parity_wins)
    
    """ Returns a single record if there is no parity or the parity checks out, otherwise None.
    """
    def try_parse(self, by):
        rec = BinaryRecord.Record()
        fields = struct.unpack(self.read_struct_format, by if sys.version_info >= (3, 0) else str(by))
        
        i = 0
        for bf in self.binary_fields:
            rec.__dict__[bf.name] = fields[i] if bf.count == 1 else fields[i:i + bf.count]
            i += bf.count
        
        if self.parity_size == 0:
            return rec
        
        parity = reduce(operator.xor, by[:self.data_byte_count - self.parity_size], 0)
        
        # self.parity_size != 0 implies that there is a 'parity' field
        return rec if parity == rec.__dict__[BinaryRecord._parity_name] else None
    
    """ Returns an array of records, with properties according to the BinaryField list passed to
        the ctor. If the BinaryField has count=1, the property will be a scalar; otherwise it will
        be a list.
    """
    def decode(self, by):
        if sys.version_info < (3, 0) and type(by) is str:
            by = bytearray(by)
        
        records = []
        i = 0
        
        while i < len(by):
            if self.start_byte_idx < len(self.start_bytes):
                if by[i] == self.start_bytes[self.start_byte_idx]:
                    self.start_byte_idx += 1
                else:
                    self.start_byte_idx = 0
                    self.byte_seeks += 1
                
                i += 1
            else:
                next_i = min(len(by), i + self.data_byte_count - len(self.buffer))
                self.buffer.extend(by[i:next_i])
                
                if len(self.buffer) == self.data_byte_count:
                    rec = self.try_parse(self.buffer)
                    self.start_byte_idx = 0
                    
                    if rec != None:
                        if self.parity_size > 0:
                            self.parity_wins += 1
                        records.append(rec)
                        i = next_i
                    else:
                        if self.parity_size > 0:
                            self.parity_fails += 1
                        by = self.buffer + by[next_i:]
                        i = 0

                    self.buffer = bytearray() # or should I erase it?
                else:
                    # not enough bytes left to form a record
                    break
        
        return records


""" Example:
from binarydecoder import BinaryRecord, BinaryField
import sys

binary_fields = [
    BinaryField("ticks",    "ulong", 1),
    BinaryField("counter",  "uint",  1),
    BinaryField("accel",    "int",   3),
    BinaryField("gyro",     "int",   3),
    BinaryField("mag",      "int",   3),
    BinaryField("pressure", "float", 1),
    BinaryField("parity",   "byte",  1)
    ]

by = b'TX_sensors\r\n\x02\x02\x02\x02\x02\x02\x02\x02\x02\x02\x04\x02\xe4<\x00\x00\x01\x00%\x01S\xffO\x11\x00\x00\x00\x00\x00\x00\x93\xff>\xff\xa4\x00\x00\x9b\x8eGT\x02\xa8\xbc\x01\x00\x02\x00\x16\x01N\xff<\x11%\x00\xe0\xffU\x00\x8c\xff?\xff\x9e\x00\x803\xc0G\xea\x02PC\x03\x00\x03\x00\x18\x01>\xffD\x11"\x00\xdb\xff\x1f\x00\x91\xff=\xff\xa2\x00\x004\xc0G:\x02\xf8\xc9\x04\x00\x04\x00 \x01Q\xffQ\x11"\x00\xdb\xff\x1f\x00\x90\xff<\xff\xa1\x00\x004\xc0GY\x02\xa0L\x06\x00\x05\x00*\x01O\xffD\x11"\x00\xdb\xff \x00\x90\xff>\xff\xa2\x00\x804\xc0G8\x02H\xd3\x07\x00\x06\x00%\x01E\xffF\x11"\x00\xdb\xff\x1f\x00\x8e\xff=\xff\xa2\x00\x806\xc0Gj\x02\xf0Y\t\x00\x07\x00\'\x01F\xff7\x11"\x00\xdc\xff \x00\x92\xff>\xff\xa2\x00\x808\xc0G\x0e\x02\x98\xdc\n\x00\x08\x008\x01X\xff<\x11!\x00\xdb\xff\x1f\x00\x8f\xff;\xff\xa3\x00\x806\xc0G\xc9\x02@c\x0c\x00\t\x00\x16\x01P\xff6\x11"\x00\xdb\xff\x1f\x00\x93\xff<\xff\x9f\x00\x002\xc0G%\x02\xe8\xe9\r\x00\n\x00"\x01:\xffP\x11"\x00\xdb\xff \x00\x90\xff<\xff\xa2\x00\x002\xc0G<\x02\x90l\x0f\x00\x0b\x002\x01L\xff=\x11"\x00\xdb\xff \x00\x8f\xff?\xff\xa0\x00\x003\xc0G\xd6\x028\xf3\x10\x00\x0c\x00\'\x01J\xffK\x11"\x00\xdc\xff\x1f\x00\x91\xff:\xff\xa0\x00\x003\xc0G\xbf\x02\xe0y\x12\x00\r\x00#\x01O\xff>\x11"\x00\xdb\xff\x1f\x00\x91\xff:\xff\xa4\x00\x003\xc0G\x99\x02\x88\xfc\x13\x00\x0e\x005\x01N\xff^\x11#\x00\xdb\xff \x00\x8f\xff@\xff\x9f\x00\x003\xc0G`\x020\x83\x15\x00\x0f\x00\x1f\x01L\xff7\x11"\x00\xdc\xff\x1f\x00\x90\xff<\xff\xa1\x00\x803\xc0G\x05\x02\xd8\t\x17\x00\x10\x003\x01^\xffU\x11"\x00\xdb\xff\x1f\x00\x91\xff>\xff\xa0\x00\x805\xc0G%\x02\x80\x8c\x18\x00\x11\x00\x1b\x01F\xffB\x11"\x00\xdc\xff \x00\x92\xff@\xff\xa2\x00\x807\xc0G\x94\x02(\x13\x1a\x00\x12\x001\x01J\xffM\x11"\x00\xdb\xff\x1f\x00\x94\xff>\xff\xa2\x00\x006\xc0GJ\x02\xd0\x99\x1b\x00\x13\x00@\x01A\xff=\x11"\x00\xdb\xff \x00\x91\xff=\xff\xa0\x00\x802\xc0G\x8d\x02x\x1c\x1d\x00\x14\x00+\x01?\xffH\x11"\x00\xdb\xff \x00\x90\xff>\xff\xa0\x00\x802\xc0G\xc3\x02 \xa3\x1e\x00\x15\x00!\x01O\xffB\x11"\x00\xdb\xff \x00\x92\xff=\xff\xa3\x00\x804\xc0GR\x02\xc8) \x00\x16\x00(\x01]\xff8\x11#\x00\xdb\xff\x1f\x00\x8f\xff>\xff\xa1\x00\x804\xc0GN\x02p\xac!\x00\x17\x00*\x01D\xffO\x11"\x00\xdb\xff \x00\x91\xff>\xff\xa1\x00\x001\xc0G\xba\x02\x183#\x00\x18\x00\'\x01C\xffI\x11"\x00\xdb\xff\x1f\x00\x8f\xff?\xff\xa3\x00\x001\xc0Gn\x02\xc0\xb9$\x00\x19\x00/\x01>\xff6\x11"\x00\xdb\xff\x1f\x00\x90\xff;\xff\xa2\x00\x805\xc0G\xae\x02h<&\x00\x1a\x000\x01L\xffB\x11"\x00\xdb\xff\x1f\x00\x90\xff=\xff\xa1\x00\x805\xc0G\x9e\x02\x10\xc3\'\x00\x1b\x00\x1c\x019\xffD\x11!\x00\xdb\xff \x00\x92\xff=\xff\xa2\x00\x804\xc0Gz\x02\xb8I)\x00\x1c\x00\x1f\x01Q\xffB\x11"\x00\xdb\xff\x1f\x00\x90\xff=\xff\xa2\x00\x806\xc0G\x00\x02`\xcc*\x00\x1d\x00%\x01@\xffE\x11"\x00\xdc\xff\x1f\x00\x93\xff:\xff\xa1\x00\x804\xc0Gq\x02\x08S,\x00\x1e\x00\x1e\x01P\xff:\x11#\x00\xdb\xff\x1f\x00\x91\xff;\xff\xa0\x00\x804\xc0G\xd3\x02\xb0\xd9-\x00\x1f\x00\x1c\x01G\xffI\x11#\x00\xdb\xff\x1f\x00\x91\xff>\xff\xa3\x00\x008\xc0G\r\x02X\\/\x00 \x00\x14\x01A\xffI\x11"\x00\xdb\xff\x1f\x00\x90\xff<\xff\xa3\x00\x006\xc0G_\x02\x00\xe30\x00!\x00$\x01N\xff=\x11"\x00\xdb\xff \x00\x92\xff>\xff\xa1\x00\x003\xc0G\xd5\x02\xa8i2\x00"\x00 \x01B\xff;\x11!\x00\xdb\xff \x00\x90\xff?\xff\xa2\x00\x005\xc0G\xfd\x02P\xec3\x00#\x00\'\x01>\xffM\x11"\x00\xdc\xff\x1f\x00\x8f\xff;\xff\xa4\x00\x806\xc0G(\x02\xf8r5\x00$\x000\x01g\xffP\x11"\x00\xdb\xff\x1f\x00\x91\xff<\xff\xa1\x00\x804\xc0GU\x02\xa0\xf96\x00%\x00)\x01A\xffL\x11"\x00\xdb\xff\x1f\x00\x90\xff>\xff\xa0\x00\x804\xc0G\xa5\x02H|8\x00&\x00\x16\x01<\xff?\x11"\x00\xdb\xff \x00\x90\xff>\xff\xa0\x00\x804\xc0G\xcb\x02\xf0\x02:\x00\'\x00+\x01H\xff8\x11"\x00\xdb\xff \x00\x90\xff<\xff\xa3\x00\x003\xc0G\xc6\x02\x98\x89;\x00(\x00%\x01U\xffH\x11"\x00\xdb\xff\x1f\x00\x8f\xff=\xff\xa2\x00\x80/\xc0G\xf4\x02@\x0c=\x00)\x00\x1d\x01>\xffD\x11!\x00\xdb\xff\x1f\x00\x8e\xff?\xff\xa0\x00\x00/\xc0Gs\x02\xe8\x92>\x00*\x00\x1c\x01C\xff<\x11"\x00\xdb\xff \x00\x92\xff;\xff\xa4\x00\x802\xc0G\xfc\x02\x90\x19@\x00+\x00 \x01F\xffH\x11!\x00\xda\xff \x00\x90\xff=\xff\xa1\x00\x801\xc0G=\x028\x9cA\x00,\x00#\x019\xffE\x11!\x00\xdb\xff \x00\x91\xff=\xff\x9f\x00\x801\xc0GY\x02\xe0"C\x00-\x00#\x01?\xff2\x11"\x00\xdb\xff\x1f\x00\x92\xff=\xff\xa2\x00\x802\xc0GL'

br = BinaryRecord([0x02], binary_fields, [BinaryFieldType("float", 4, "f", '%.4f')])
#by = open(sys.argv[1], 'rb').read()
recs = br.decode(by)

print(br.status())
print('\n'.join(map(br.to_string, recs)))
"""