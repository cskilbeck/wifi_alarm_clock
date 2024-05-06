//////////////////////////////////////////////////////////////////////
// Janky one-off for converting .bitmapfont files to .c and .h

using System.Xml.Linq;

//////////////////////////////////////////////////////////////////////

static int get_attr(XElement x, string name)
{
    var attr = x.Attribute(name);
    if (attr != null)
    {
        return (int)(float.Parse(attr.Value) + 0.5f);
    }
    throw new Exception($"Missing attribute {name} in {x.Name}");
}

//////////////////////////////////////////////////////////////////////

string[] arguments = Environment.GetCommandLineArgs();

if (arguments.Length != 4)
{
    throw new Exception("Usage FontParser [input.bitmapfont] [output.c] [output.h]");
}

string filename = Path.GetFileNameWithoutExtension(arguments[1]);
string font_name = filename + "_font";

string output_c_filename = arguments[2];
string output_h_filename = arguments[3];

//////////////////////////////////////////////////////////////////////

XElement font = XElement.Load(arguments[1]);

List<Glyph> glyphs = new List<Glyph>();
List<Graphic> graphics = new List<Graphic>();

int graphic_index = 0;

int height = get_attr(font, "Height");

var glyph_elements = font.Elements("Glyphs").Elements("Glyph");

foreach (var glyph_element in glyph_elements)
{
    Glyph glyph = new()
    {
        c = get_attr(glyph_element, "char"),
        image_count = get_attr(glyph_element, "images"),
        advance = get_attr(glyph_element, "advance"),
        graphic_base = graphic_index,
    };

    glyphs.Add(glyph);

    var graphic_elements = glyph_element.Elements("Graphic");

    if (graphic_elements == null)
    {
        throw new Exception($"Missing graphics for glyph {glyph.c}");
    }

    if (graphic_elements.Count() != glyph.image_count)
    {
        throw new Exception($"Wrong number of graphics for glyph {glyph.c}, expected {glyph.image_count}, got {graphic_elements.Count()}");
    }

    foreach (var graphic_element in graphic_elements)
    {
        graphics.Add(new()
        {
            offset_x = get_attr(graphic_element, "offsetX"),
            offset_y = get_attr(graphic_element, "offsetY"),
            x = get_attr(graphic_element, "x"),
            y = get_attr(graphic_element, "y"),
            width = get_attr(graphic_element, "w"),
            height = get_attr(graphic_element, "h"),
            page = get_attr(graphic_element, "page"),
        });
    }

    graphic_index = graphics.Count;
}

//////////////////////////////////////////////////////////////////////
// make a lookup table for the graphics

var lookup = new int[256];
var advance = new int[256];

for (int i = 0; i < lookup.Length; i++)
{
    lookup[i] = -1;
    advance[i] = 0;
}

foreach (var glyph in glyphs)
{
    if (glyph.image_count != 0)
    {
        lookup[glyph.c] = glyph.graphic_base;
    }
    advance[glyph.c] = glyph.advance;
}

//////////////////////////////////////////////////////////////////////

StreamWriter output_c_file = new(output_c_filename);

output_c_file.WriteLine($"// FONT: {font_name} has {graphics.Count} graphics");
output_c_file.WriteLine();
output_c_file.WriteLine("// struct font_graphic {");
output_c_file.WriteLine("//     int8_t offset_x;");
output_c_file.WriteLine("//     int8_t offset_y;");
output_c_file.WriteLine("//     uint8_t x;");
output_c_file.WriteLine("//     uint8_t y;");
output_c_file.WriteLine("//     uint8_t width;");
output_c_file.WriteLine("//     uint8_t height;");
output_c_file.WriteLine("// };");
output_c_file.WriteLine();
output_c_file.WriteLine("// struct font_data {");
output_c_file.WriteLine("//     font_graphic *graphics;");
output_c_file.WriteLine("//     int16_t *lookup;");
output_c_file.WriteLine("//     uint8_t *advance;");
output_c_file.WriteLine("//     int height;");
output_c_file.WriteLine("//     int num_graphics;");
output_c_file.WriteLine("//     int num_lookups;");
output_c_file.WriteLine("//     int num_advances;");
output_c_file.WriteLine("// };");
output_c_file.WriteLine();

int columns = 16;

output_c_file.WriteLine($"font_graphic {font_name}_graphics[{graphics.Count}] = {{");
foreach (var g in graphics)
{
    output_c_file.WriteLine($"    {{ {g.offset_x}, {g.offset_y}, {g.x}, {g.y}, {g.width}, {g.height} }},");
}
output_c_file.WriteLine("};");
output_c_file.WriteLine();
output_c_file.Write($"int16_t {font_name}_lookup[{lookup.Length}] = {{");
for (int index = 0; index < lookup.Length; index++)
{
    var sep = "";
    if ((index % columns) == 0)
    {
        sep = "\n    ";
    }
    output_c_file.Write($"{sep}{lookup[index],3},");
}
output_c_file.WriteLine("\n};");
output_c_file.WriteLine();
output_c_file.Write($"uint8_t {font_name}_advance[{lookup.Length}] = {{");
for (int index = 0; index < lookup.Length; index++)
{
    var sep = "";
    if ((index % columns) == 0)
    {
        sep = "\n    ";
    }
    output_c_file.Write($"{sep}{advance[index],3},");
}
output_c_file.WriteLine("\n};");
output_c_file.WriteLine();
output_c_file.WriteLine($"font_data {font_name} = {{");
output_c_file.WriteLine($"    {font_name}_graphics,");
output_c_file.WriteLine($"    {font_name}_lookup,");
output_c_file.WriteLine($"    {font_name}_advance,");
output_c_file.WriteLine($"    {height}, // height");
output_c_file.WriteLine($"    {graphics.Count}, // num_graphics");
output_c_file.WriteLine($"    {lookup.Length}, // num_lookups");
output_c_file.WriteLine($"    {advance.Length} // num_advances");
output_c_file.WriteLine("};");
output_c_file.WriteLine();
output_c_file.WriteLine($"extern const uint8_t {filename}_png_start[] asm(\"_binary_{filename}0_png_start\");");
output_c_file.WriteLine($"extern const uint8_t {filename}_png_end[] asm(\"_binary_{filename}0_png_end\");");

output_c_file.Flush();
output_c_file.Close();

//////////////////////////////////////////////////////////////////////

StreamWriter output_h_file = new(output_h_filename);

output_h_file.WriteLine($"// FONT: {font_name} has {graphics.Count} graphics");
output_h_file.WriteLine();
output_h_file.WriteLine($"extern font_graphic {font_name}_graphics[{graphics.Count}];");
output_h_file.WriteLine($"extern int16_t {font_name}_lookup[{lookup.Length}];");
output_h_file.WriteLine($"extern uint8_t {font_name}_advance[{lookup.Length}];");
output_h_file.WriteLine($"extern font_data {font_name};");
output_h_file.WriteLine();
output_h_file.WriteLine($"extern const uint8_t {font_name}_png_start[];");
output_h_file.WriteLine($"extern const uint8_t {font_name}_png_end[];");
output_h_file.WriteLine($"#define {font_name}_png_size ((size_t)({font_name}_png_end - {font_name}_png_start))");

output_h_file.Flush();
output_h_file.Close();

//////////////////////////////////////////////////////////////////////

struct Glyph
{
    public int c;
    public int image_count;
    public int advance;
    public int graphic_base;
};

//////////////////////////////////////////////////////////////////////

struct Graphic
{
    public int offset_x;
    public int offset_y;
    public int x;
    public int y;
    public int width;
    public int height;
    public int page;
};

