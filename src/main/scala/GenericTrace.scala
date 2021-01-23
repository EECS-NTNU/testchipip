package testchipip

import chipsalliance.rocketchip.config.Field
import chisel3._
import freechips.rocketchip.diplomacy.{BundleBridgeNexus, LazyModuleImp}
import freechips.rocketchip.rocket.GenericTrace
import freechips.rocketchip.subsystem.HasTiles
import freechips.rocketchip.util.HeterogeneousBag


// A per-tile interface that includes the tile's clock and reset
class TileGenericTraceIO(val bitWidth: Int) extends Bundle {
  val clock = Clock()
  val reset = Bool()
  val data = new GenericTrace(bitWidth)
}

// The IO matched on by the GenericTrace bridge: a wrapper around a heterogenous
// bag of TileGenericTraceIO. Each entry is trace associated with a single tile
class GenericTraceOutputTop(val bitWidths: Seq[Int]) extends Bundle {
  val generic_traces = Output(HeterogeneousBag(bitWidths.map(w => new TileGenericTraceIO(w))))
}

object GenericTraceOutputTop {
  def apply(proto: Seq[GenericTrace]): GenericTraceOutputTop =
    new GenericTraceOutputTop(proto.map(t => t.bitWidth))
}

// Use this trait:
trait CanHaveGenericTraceIO { this: HasTiles =>
  val module: CanHaveGenericTraceIOModuleImp

  // Bind all the trace nodes to a BB; we'll use this to generate the IO in the imp
  val genericTraceNexus = BundleBridgeNexus[GenericTrace]
  tiles.foreach { genericTraceNexus := _.genericTraceNode }
}
case class GenericTracePortParams(print: Boolean = false)
object GenericTracePortKey extends Field[Option[GenericTracePortParams]](None)

trait CanHaveGenericTraceIOModuleImp extends LazyModuleImp {
  val outer: CanHaveGenericTraceIO with HasTiles

    val genericTraceIO = p(GenericTracePortKey) map ( traceParams => {
    val genericTraceSeq = (outer.genericTraceNexus.in.map(_._1))
    val tio = IO(Output(GenericTraceOutputTop(genericTraceSeq)))

    (tio.generic_traces zip (outer.tiles zip genericTraceSeq)).foreach { case (port, (tile, gentrace)) =>
      port.clock := tile.module.clock
      port.reset := tile.module.reset.asBool
      port.data := gentrace
    }

    if (traceParams.print) {
      for ((trace, idx) <- tio.generic_traces.zipWithIndex ) {
        withClockAndReset(trace.clock, trace.reset) {
          when (trace.data.valid) {
            printf(s"GENERIC TRACE ${idx}: %x\n", trace.data.bits.asUInt())
          }
        }
      }
    }
    tio
  })
}



