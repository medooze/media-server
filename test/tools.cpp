#include "test.h"
#include "log.h"
#include "acumulator.h"
#include "MovingCounter.h"
#include "CircularBuffer.h"

class ToolsPlan : public TestPlan
{
public:
        ToolsPlan() : TestPlan("Tools test plan")
        {

        }

        virtual void Execute()
        {
                testAccumulator();
                testCircularBuffer();
        }

        void testAccumulator()
        {

                Acumulator acu(10);
                MovingMinCounter<DWORD> min(10);
                MovingMaxCounter<DWORD> max(10);


                for (uint64_t i = 10; i < 1E6; i++)
                {
                        DWORD val = rand();
                        acu.Update(i, val);
                        min.Add(i, val);
                        max.Add(i, val);

                        assert(acu.GetMaxValueInWindow() == *max.GetMax());
                        assert(acu.GetMinValueInWindow() == *min.GetMin());


                }
        }

        void testCircularBuffer()
        {
                Log("-testCircularBuffer\n");

                {

                        Log("-testCircularBuffer secuential\n");
                        CircularBuffer<uint16_t, uint8_t, 10> buffer;

                        assert(!buffer.IsPresent(0));
                        for (uint16_t i = 0; i < 399; ++i)
                        {
                                buffer.Set(i, i);
                                Log("%i %i %i %i %d\n", i, buffer.Get(i).value(), buffer.GetFirstSeq(), buffer.GetLastSeq(), buffer.GetLength());
                                assert(buffer.IsPresent(i)); assert(buffer.Get(i).value() == i); assert(buffer.GetLastSeq() == i % 256);
                        }

                        assert(!buffer.Set(buffer.GetLastSeq() - 11, 0));
                }

                {
                        Log("-testCircularBuffer evens\n");
                        CircularBuffer<uint16_t, uint8_t, 10> buffer;

                        assert(!buffer.IsPresent(0));
                        for (uint16_t i = 0; i < 399; ++i)
                        {
                                buffer.Set(i * 2, i);
                                Log("%i %i %i %i %d\n", i, buffer.Get(i * 2).value(), buffer.GetFirstSeq(), buffer.GetLastSeq(), buffer.GetLength());
                                assert(buffer.IsPresent(i * 2)); assert(buffer.Get(i * 2).value() == i); assert(buffer.GetLastSeq() == (i * 2) % 256);
                                if (i) assert(!buffer.IsPresent(i * 2 - 1));
                        }

                        assert(!buffer.Set(buffer.GetLastSeq() - 11, 0));
                }


                {
                        Log("-testCircularBuffer big jumps\n");
                        CircularBuffer<uint16_t, uint8_t, 10> buffer;

                        assert(!buffer.IsPresent(0));
                        for (uint16_t i = 0; i < 399; ++i)
                        {
                                buffer.Set(i * 20, i);
                                Log("%i %i %i %i %d\n", i, buffer.Get(i * 20).value(), buffer.GetFirstSeq(), buffer.GetLastSeq(), buffer.GetLength());
                                assert(buffer.IsPresent(i * 20)); assert(buffer.Get(i * 20).value() == i); assert(buffer.GetLastSeq() == (i * 20) % 256);
                                if (i) assert(!buffer.IsPresent(i * 20 - 1));
                        }

                        assert(!buffer.Set(buffer.GetLastSeq() - 11, 0));
                }
        }

};

ToolsPlan tools;

