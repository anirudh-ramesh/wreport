#include "config.h"

#include "opcode.h"
#include "bulletin.h"
#include "bulletin/buffers.h"
#include "bulletin/internals.h"

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>	/* isspace */
#include <stdlib.h>	/* malloc */
#include <string.h>	/* memcpy */
#include <math.h>	/* NAN */
#include <assert.h>	/* NAN */
#include <errno.h>	/* NAN */

// #define TRACE_ENCODER

#ifdef TRACE_ENCODER
#define TRACE(...) fprintf(stderr, __VA_ARGS__)
#define IFTRACE if (1)
#else
#define TRACE(...) do { } while (0)
#define IFTRACE if (0)
#endif

using namespace std;

namespace wreport {

namespace {

struct DDSEncoder : public bulletin::BaseParser
{
    bulletin::CrexOutput& ob;

    DDSEncoder(Bulletin& b, bulletin::CrexOutput& ob, unsigned subset_no)
        : BaseParser(b, subset_no), ob(ob)
    {
        TRACE("start_subset %u\n", subset_no);

        /* Encode the subsection terminator */
        if (subset_no > 0)
            ob.raw_append("+\r\r\n", 4);
    }
    virtual ~DDSEncoder() {}

    void do_attr(Varinfo info, unsigned var_pos, Varcode attr_code) override
    {
        throw error_unimplemented("do_attr");
    }

    void define_variable(Varinfo info) override
    {
        const Var& var = get_var();
        IFTRACE {
            TRACE("encode_var ");
            var.print(stderr);
        }
        ob.append_var(info, var);
    }

    const Var& define_semantic_variable(Varinfo info) override
    {
        const Var& var = get_var();
        IFTRACE {
            TRACE("encode_semantic_var ");
            var.print(stderr);
        }
        switch (info->code)
        {
            case WR_VAR(0, 31, 1):
            case WR_VAR(0, 31, 2):
            case WR_VAR(0, 31, 11):
            case WR_VAR(0, 31, 12):
            {
                unsigned count = var.enqi();

                /* Encode the repetition count */
                ob.raw_append(" ", 1);
                ob.encode_check_digit();
                ob.raw_appendf("%04u", count);
                break;
            }
            default:
                ob.append_var(info, var);
                break;
        }
        return var;
    }
};


struct Encoder
{
    // Input message data
    CrexBulletin& in;
    // Output decoded variables
    bulletin::CrexOutput out;

    // Offset of the start of CREX section 1
    int sec1_start = 0;
    // Offset of the start of CREX section 2
    int sec2_start = 0;
    // Offset of the start of CREX section 3
    int sec3_start = 0;
    // Offset of the start of CREX section 4
    int sec4_start = 0;

    // Subset we are encoding
    const Subset* subset = nullptr;

    Encoder(CrexBulletin& in, std::string& out)
        : in(in), out(out)
    {
    }

    void encode_sec1()
    {
        out.raw_appendf("T%02d%02d%02d A%03d%03d",
                in.master_table_number,
                in.edition,
                in.table,
                in.type,
                in.localsubtype);

        /* Encode the data descriptor section */

		for (vector<Varcode>::const_iterator i = in.datadesc.begin();
				i != in.datadesc.end(); ++i)
		{
			char prefix;
			switch (WR_VAR_F(*i))
			{
				case 0: prefix = 'B'; break;
				case 1: prefix = 'R'; break;
				case 2: prefix = 'C'; break;
				case 3: prefix = 'D'; break;
				default: prefix = '?'; break;
			}

			// Don't put delayed replication counts in the data section
			if (WR_VAR_F(*i) == 0 && WR_VAR_X(*i) == 31 && WR_VAR_Y(*i) < 3)
				continue;

            out.raw_appendf(" %c%02d%03d", prefix, WR_VAR_X(*i), WR_VAR_Y(*i));
        }

        if (out.has_check_digit)
        {
            out.raw_append(" E", 2);
            out.expected_check_digit = 1;
        }

        out.raw_append("++\r\r\n", 5);
    }

    void run()
    {
        /* Encode section 0 */
        out.raw_append("CREX++\r\r\n", 9);

        /* Encode section 1 */
        sec1_start = out.buf.size();
        encode_sec1();
        TRACE("SEC1 encoded as [[[%s]]]", out.buf.substr(sec1_start).c_str());

        /* Encode section 2 */
        sec2_start = out.buf.size();

        // Encode all subsets
        for (unsigned i = 0; i < in.subsets.size(); ++i)
        {
            DDSEncoder e(in, out, i);
            e.run();
        }
        out.raw_append("++\r\r\n", 5);

        TRACE("SEC2 encoded as [[[%s]]]", out.buf.substr(sec2_start).c_str());

        /* Encode section 3 */
        sec3_start = out.buf.size();
        /* Nothing to do, as we have no custom section */

        /* Encode section 4 */
        sec4_start = out.buf.size();
        out.raw_append("7777\r\r\n", 7);
    }
};


} // Unnamed namespace

void CrexBulletin::encode(std::string& buf)
{
    Encoder e(*this, buf);
    e.run();
    //out.encoding = CREX;
}

}
