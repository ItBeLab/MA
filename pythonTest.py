from MA import *
import random
import gc
import os
import math
from bokeh.plotting import figure, output_file, show
from bokeh.layouts import row, column, layout
from bokeh.palettes import d3
from bokeh.models import LinearAxis, Range1d, LogColorMapper, FixedTicker, BasicTicker, Grid
from bokeh.models.formatters import FuncTickFormatter
from bokeh.layouts import gridplot
from bokeh.io import save
import numpy as np
import colorsys
from statistics import mean, median, stdev
from sys import stderr

def light_spec_approximation(x):
    #map input [0, 1] to wavelength [350, 645]
    w = 370 + x * (645-370)
    r = 0.0
    g = 0.0
    b = 0.0
    if w < 440:
        r = -(w - 440.) / (440. - 380.)
        b = 1.0
    elif w >= 440 and w < 490:
        g = (w - 440.) / (490. - 440.)
        b = 1.0
    elif w >= 490 and w < 510:
        g = 1.0
        b = -(w - 510.) / (510. - 490.)
    elif w >= 510 and w < 580:
        r = (w - 510.) / (580. - 510.)
        g = 1.0
    elif w >= 580 and w < 645:
        r = 1.0
        g = -(w - 645.) / (645. - 580.)
    elif w >= 645:
        r = 1.0

    #intensity
    i = 1.0
    if w > 650:
        i = .3 + .7*(780-w)/(780-650)
    elif w < 420:
        i = .3 + .7*(w-380)/(420-380)

    #gamma
    m = .8

    return (i*r**m, i*g**m, i*b**m)

def heatmap_palette(scheme, num_colors):
    def format(rgb):
        def clamp(x):
            return max(0, min(x, 255))
        red, green, blue = rgb
        return "#{0:02x}{1:02x}{2:02x}".format(clamp(int(red * 255)), clamp(int(green * 255)),
                                               clamp(int(blue * 255)))
    return [format(scheme(x)) for x in np.linspace(0, 1, num_colors)]

human_genome = "/mnt/ssd0/genome/human"

## @brief Yield successive n-sized chunks from l.
def chunks(l, n):
    if n >= len(l):
        yield l
    else:
        for i in range(0, len(l), n):
            yield l[i:i + n]


#creating samples int the database
#createSampleQueries(human_genome, db_name, 1000, 100, 256)

def test_my_approach(
        db_name,
        reference,
        name,
        max_hits=100,
        num_strips=10,
        complete_seeds=False,
        use_chaining=False,
        local=True,
        reseed=False,
        full_analysis=False,
        do_minimizers=False
        #,optimistic_gap_estimation=False
    ):
    print("collecting samples (" + name + ") ...")

    all_queries = getNewQueries(db_name, name, reference, give_orig_pos=True, give_orig_size=True)
    #queries = getQueriesFor(db_name, reference, 40, 0, size)

    print("having ", len(all_queries), " samples total (", name, ") ...")
    if len(all_queries) == 0:
        print("no new queries ... done")
        return

    runtimes = {
        'seeding': 0,
        'seed processing: filtering': 0,
        'seed processing: coupling': 0,
        'optimal alignment': 0
    }
    collect_ids = []

    num_covered_irelevant_seeds = 0
    num_irelevant_seeds = 0
    num_seeds_total = 0
    num_soc_seeds = 0
    num_coupled_seeds = 0
    warn_once = True

    extract_size = 2**15
    # break samples into chunks of 2^15
    for index, queries, in enumerate(chunks(all_queries, extract_size)):
        print("extracting", len(queries), "samples", index, "/",
              len(all_queries)/extract_size, "(", name, ") ...")

        print("setting up (", name, ") ...")

        ref_pack = Pack()
        ref_pack.load(reference)
        ref_pledge = Pledge(Pack())
        ref_pledge.set(ref_pack)

        fm_pledge = None

        if do_minimizers:
            minimizersHash = MinimizersHash().from_file(reference + ".maRef")
            minimizersHash.verify(ref_pack)# self tests...
            fm_pledge = Pledge(MinimizersHash())
            fm_pledge.set(minimizersHash)
        else:
            fm_index = FMIndex()
            fm_index.load(reference)
            fm_pledge = Pledge(FMIndex())
            fm_pledge.set(fm_index)

        #modules
        seeding = BinarySeeding(not complete_seeds)
        reseeding = ReSeed(max_hits)
        minimizers = Minimizers()
        minimizersExtract = MinimizersToSeeds()
        soc = StripOfConsideration(max_hits, num_strips)
        soc2 = StripOfConsideration2(max_hits, num_strips)
        ex = ExtractAllSeeds(max_hits)
        ls = LinearLineSweep()
        #ls.optimistic_gap_estimation = optimistic_gap_estimation
        couple = ExecOnVec(ls)
        chain = Chaining()
        nmw = NeedlemanWunsch(local)
        optimal = ExecOnVec(nmw)
        mappingQual = MappingQuality()

        pledges = [[], [], [], [], [], []]
        optimal_alignment_in = []
        for data in queries:
            sequence, sample_id, origin_pos, orig_size = data
            pledges[0].append(Pledge(NucSeq()))
            pledges[0][-1].set(NucSeq(sequence))
            pledges[0][-1].get().name = str(sample_id)
            if do_minimizers:
                pledges[1].append(minimizersExtract.promise_me(
                    fm_pledge, minimizers.promise_me(pledges[0][-1]), ref_pledge
                ))
                pledges[2].append(soc2.promise_me(
                    pledges[1][-1], pledges[0][-1], ref_pledge
                ))
                pledges[3].append(couple.promise_me(
                    pledges[2][-1]
                ))
            else:
                pledges[1].append(seeding.promise_me(
                    fm_pledge, pledges[0][-1]
                ))
                if reseed:
                    pledges[1][-1] = reseeding.promise_me(
                        fm_pledge, pledges[1][-1], pledges[0][-1]
                    )
                if use_chaining:
                    pledges[2].append(ex.promise_me(
                        pledges[1][-1], fm_pledge
                    ))
                    pledges[3].append(chain.promise_me(
                        pledges[2][-1]
                    ))
                else:
                    pledges[2].append(soc.promise_me(
                        pledges[1][-1], pledges[0][-1], ref_pledge, fm_pledge
                    ))
                    pledges[3].append(couple.promise_me(
                        pledges[2][-1]
                    ))
            pledges[4].append(optimal.promise_me(
                pledges[3][-1], pledges[0][-1], ref_pledge
            ))
            pledges[5].append(mappingQual.promise_me(
                pledges[0][-1], pledges[4][-1]
            ))

            optimal_alignment_in.append((Pledge(NucSeq()), Pledge(NucSeq())))

        smw = SMW(full_analysis)
        optimal_alignment_out = []
        for a, b in optimal_alignment_in:
            optimal_alignment_out.append(smw.promise_me(a, b))


        print("computing (", name, ") ...")
        Pledge.simultaneous_get(pledges[-1], 32)


        print("setting up optimal (", name, ") ...")
        for i, query_ in enumerate(queries):
            alignment = pledges[-1][i].get()[0]
            if alignment.begin_on_query == alignment.end_on_query or not full_analysis:
                optimal_alignment_in[i][0].set(NucSeq())
                optimal_alignment_in[i][1].set(NucSeq())
                continue
            query = query_[0]
            optimal_alignment_in[i][0].set(
                NucSeq(query[alignment.begin_on_query : alignment.end_on_query])
            )
            optimal_alignment_in[i][1].set(
                ref_pack.extract_from_to(alignment.begin_on_ref, alignment.end_on_ref)
            )
        print("computing optimal (", name, ") ...")
        Pledge.simultaneous_get(optimal_alignment_out, 32)

        print("extracting results (", name, ") ...")
        result = []
        for i, alignments in enumerate(pledges[-1]):
            alignment = alignments.get()[0]
            alignment2 = None
            if len(alignments.get()) > 1:
                alignment2 = alignments.get()[1]
            optimal_alignment = None
            if len(optimal_alignment_out[i].get()) > 0:
                optimal_alignment = optimal_alignment_out[i].get()[0]
                if optimal_alignment.end_on_query == 0 or optimal_alignment.end_on_ref == 0:
                    optimal_alignment = None
                else:
                    # adjust the scores to the slice that 
                    # was actually computed with the optimal alignment
                    optimal_alignment.begin_on_query += alignment.begin_on_query
                    optimal_alignment.end_on_query += alignment.begin_on_query
                    optimal_alignment.begin_on_ref += alignment.begin_on_ref
                    optimal_alignment.end_on_ref += alignment.begin_on_ref
            #print(alignment.stats.name)
            sample_id = int(alignment.stats.name)
            collect_ids.append(sample_id)

            total_time = 0
            runtimes["seeding"] += pledges[1][i].exec_time
            total_time += pledges[1][i].exec_time
            pledges[1][i].exec_time = 0
            runtimes["seed processing: filtering"] += pledges[2][i].exec_time
            total_time += pledges[2][i].exec_time
            pledges[2][i].exec_time = 0
            runtimes["seed processing: coupling"] += pledges[3][i].exec_time
            total_time += pledges[3][i].exec_time
            pledges[3][i].exec_time = 0
            runtimes["optimal alignment"] += pledges[4][i].exec_time
            total_time += pledges[4][i].exec_time
            pledges[4][i].exec_time = 0

            max_nmw_area = 0
            nmw_area = 0
            max_diag_deviation_percent = 0.0
            max_diag_deviation = 0
            curr_diag_deviation = 0
            curr_max_diag_deviation = 0
            gap_size = 0
            cur_nmw_h = 0
            cur_nmw_w = 0
            seed_coverage = 0.0

            def print_alignments():
                print("MA alignment:")
                AlignmentPrinter().execute(
                    alignment,
                    pledges[0][i].get(),
                    ref_pack
                )
                if optimal_alignment != None:
                    print("SA alignment:")
                    AlignmentPrinter().execute(
                        optimal_alignment,
                        pledges[0][i].get(),
                        ref_pack
                    )

            if optimal_alignment != None and alignment.get_score() > optimal_alignment.get_score():
                print("WARNING: alignment computed better than optimal score",
                      alignment.get_score(), optimal_alignment.get_score()
                     )
                print_alignments()
                sw = SMW(True)
                sw.print = True
                sw.execute(
                    pledges[0][i].get(),
                    ref_pack.extract_from_to(alignment.begin_on_ref, alignment.end_on_ref)
                )
            if (warn_once and local and optimal_alignment != None and
                    alignment.get_score() < optimal_alignment.get_score()):
                warn_once = False
                print("got worse than optimal score", alignment.get_score(),
                      optimal_alignment.get_score()
                     )
                print("this warning is just printed once")
                print_alignments()

            seed_coverage_soc = 0.0
            if full_analysis:
                for match_type in alignment.extract():
                    if match_type == MatchType.seed:
                        seed_coverage += 1.0
                        if (
                                gap_size > 100 and
                                nmw.penalty_missmatch * gap_size < nmw.penalty_gap_open +
                                nmw.penalty_gap_extend * gap_size
                            ):
                            if (
                                    float(abs(curr_max_diag_deviation)) / gap_size >
                                    max_diag_deviation_percent
                                ):
                                max_diag_deviation_percent = float(abs(curr_max_diag_deviation))
                                max_diag_deviation_percent /= gap_size
                        if abs(curr_max_diag_deviation) > max_diag_deviation:
                            max_diag_deviation = abs(curr_max_diag_deviation)
                        curr_diag_deviation = 0
                        curr_max_diag_deviation = 0
                        gap_size = 0
                        nmw_area += cur_nmw_h * cur_nmw_w
                        cur_nmw_h = 0
                        cur_nmw_w = 0
                    else:
                        gap_size += 1
                        if match_type == MatchType.insertion:
                            curr_diag_deviation += 1
                            cur_nmw_h += 1
                        elif match_type == MatchType.deletion:
                            curr_diag_deviation -= 1
                            cur_nmw_w += 1
                        elif match_type == MatchType.missmatch:
                            cur_nmw_w += 1
                            cur_nmw_h += 1
                        if curr_diag_deviation > curr_max_diag_deviation:
                            curr_max_diag_deviation = curr_diag_deviation
                        if cur_nmw_h * cur_nmw_w > max_nmw_area:
                            max_nmw_area = cur_nmw_h * cur_nmw_w

                seed_coverage /= len(pledges[0][i].get())

                #check for how many irrelevant overlapped seeds where there...
                #first get all relevant seeds:
                if not use_chaining:
                    discovered_seeds = pledges[1][i].get().extract_seeds(fm_index, max_hits, True)
                    soc_seeds = pledges[2][i].get()[alignment.stats.index_of_strip]
                    num_soc_seeds += len(soc_seeds)
                    num_coupled_seeds += len(pledges[3][i].get()[alignment.stats.index_of_strip])
                    num_seeds_total += pledges[1][i].get().num_seeds(fm_index, max_hits)
                    #compute the area covered by relevant seeds
                    covered_area = [-1]*len(pledges[0][i].get())
                    for seed in discovered_seeds:
                        for pos in range(seed.start, seed.start + seed.size):
                            if covered_area[pos] < seed.size:
                                covered_area[pos] = seed.size
                #seed coverage after the soc
                    covered_area_soc = [False]*len(pledges[0][i].get())
                    for seed in soc_seeds:
                        for pos in range(seed.start, seed.start + seed.size):
                            covered_area_soc[pos] = True
                    for cov in covered_area_soc:
                        if cov:
                            seed_coverage_soc += 1.0
                    seed_coverage_soc /= len(pledges[0][i].get())
                #run over all discovered seeds and count the covered irelevant ones
                for seed in discovered_seeds:
                    if not (seed.start_ref >= queries[i][2] and
                            seed.start_ref + seed.size <= queries[i][2] + queries[i][3]):
                        num_irelevant_seeds += 1
                        covered = True
                        for pos in range(seed.start, seed.start + seed.size):
                            if covered_area[pos] <= seed.size:
                                covered = False
                        if covered:
                            num_covered_irelevant_seeds += 1

            sc2 = None
            if not optimal_alignment is None:
                sc2 = optimal_alignment.get_score()
            score2 = 0
            if not alignment2 is None:
                score2 = alignment2.get_score()

            num_seeds = None
            if do_minimizers:
                num_seeds = len(pledges[1][i].get())
            else:
                num_seeds = pledges[1][i].get().num_seeds(fm_index, max_hits)

            result.append(
                (
                    sample_id,
                    alignment.get_score(),
                    score2,
                    sc2,
                    alignment.begin_on_ref,
                    alignment.end_on_ref,
                    num_seeds,
                    alignment.stats.index_of_strip,
                    seed_coverage_soc,
                    seed_coverage,
                    alignment.stats.num_seeds_in_strip,
                    alignment.stats.anchor_size,
                    alignment.stats.anchor_ambiguity,
                    max_diag_deviation,
                    max_diag_deviation_percent,
                    max_nmw_area,
                    nmw_area,
                    alignment.mapping_quality,
                    total_time,
                    name
                )
            )
        print("submitting results (", name, ") ...")
        submitResults(db_name, result)

    if num_seeds_total > 0 :
        print("collected", num_irelevant_seeds,
            "irrelevant seeds. Thats", num_irelevant_seeds/len(all_queries), "per alignment and",
            100*num_irelevant_seeds/num_seeds_total, "percent")
        print("collected", num_covered_irelevant_seeds,
            "covered irrelevant seeds. Thats", num_covered_irelevant_seeds/len(all_queries),
            "per alignment and", 100*num_covered_irelevant_seeds/num_seeds_total, "percent")
    print("collected", num_seeds_total-num_irelevant_seeds, "relevant seeds")
    if num_soc_seeds > 0:
        print("having", num_soc_seeds, "seeds in the strip of consideration, having",
              num_coupled_seeds, "seeds after coupling, thats",
              100*(1-num_coupled_seeds/num_soc_seeds), "percent seeds discarded")
    last = 1
    for ele in sorted(collect_ids):
        if ele != last:
            print("Missed sample with id:", last, "having:", ele)
        last += 1
    print("total runtimes:")
    for key, value in runtimes.items():
        print(value, "(", key, ")")
    print("done")


def test_my_approaches_rele(db_name):
    """
    test_my_approach(db_name, human_genome, "non-enclosed pairs", seg=BinarySeeding(True),
                     num_anchors=200, nmw_give_up=20000)

    test_my_approach(db_name, human_genome, "non-enclosed", seg=BinarySeeding(False),
                     num_anchors=200, nmw_give_up=20000)

    #@todo BWA-MEM seeding technique

    seg = BinarySeeding(False)
    seg.do16ntEvery10ntExtension = True
    test_my_approach(db_name, human_genome, "BOWTIE 2", seg=seg, num_anchors=200, nmw_give_up=20000)

    seg2 = BinarySeeding(False)
    seg2.blasrExtension = True
    test_my_approach(db_name, human_genome, "BLASR", seg=seg2, num_anchors=200, nmw_give_up=20000)
    """

def test_my_approaches(db_name):
    full_analysis = False

    clearResults(db_name, human_genome, "MA 1")
    clearResults(db_name, human_genome, "MA 2")
    #clearResults(db_name, human_genome, "MA 1 chaining")
    #clearResults(db_name, human_genome, "MA 2 chaining")

    test_my_approach(db_name, human_genome, "MA 2", max_hits=1000, num_strips=10, complete_seeds=True, full_analysis=full_analysis)

    test_my_approach(db_name, human_genome, "MA 1", max_hits=100, num_strips=2, complete_seeds=False, full_analysis=full_analysis)

    #test_my_approach(db_name, human_genome, "MA 2", max_hits=0, num_strips=1000, complete_seeds=True, full_analysis=full_analysis)

    #test_my_approach(db_name, human_genome, "MA 2 chaining", max_hits=100, num_strips=10, complete_seeds=True, use_chaining=True, full_analysis=full_analysis)

    #test_my_approach(db_name, human_genome, "MA 1 chaining", max_hits=100, num_strips=5, complete_seeds=False, use_chaining=True, full_analysis=full_analysis)

def analyse_detailed(out_prefix, db_name):
    approaches = getApproachesWithData(db_name)
    plots = []

    for approach_ in approaches:
        strip_index = ([], [])
        seed_coverage = ([], [])
        num_seeds_tot = ([], [])
        num_seeds_strip = ([], [])
        anc_size = ([], [])
        anc_ambiguity = ([], [])
        max_diag_deviations = ([], [])
        max_diag_deviations_percent = ([], [])
        max_nmw_areas = ([], [])
        nmw_areas = ([], [])
        mapping_qual = ([], [])
        score_dif = ([], [])

        approach = approach_[0]
        for result in getResults(db_name, approach):
            score, score2, optimal_score_this_region, result_start, result_end, original_start, num_mutation, num_indels, num_seeds, mapping_quality, run_time, index_of_chosen_strip, seed_coverage_chosen_strip, seed_coverage_alignment, num_seeds_chosen_strip, anchor_size, anchor_ambiguity, max_diag_deviation, max_diag_deviation_percent, max_nmw_area, nmw_area, original_size = result

            index = 1
            if near(result_start, original_start, result_end, original_start+original_size):
                index = 0

            mapping_qual[index].append(mapping_quality)
            if not score is None and not score2 is None:
                score_dif[index].append(abs(score - score2))
            else:
                score_dif[index].append(score)
            strip_index[index].append(index_of_chosen_strip)
            if not seed_coverage_chosen_strip is None:
                seed_coverage[index].append(seed_coverage_chosen_strip / original_size)
            num_seeds_tot[index].append(num_seeds)
            num_seeds_strip[index].append(num_seeds_chosen_strip)
            anc_size[index].append(anchor_size)
            anc_ambiguity[index].append(anchor_ambiguity)
            max_diag_deviations[index].append(max_diag_deviation)
            max_diag_deviations_percent[index].append(max_diag_deviation_percent)
            max_nmw_areas[index].append(max_nmw_area)
            nmw_areas[index].append(nmw_area)

        #print("required_nmw_area", max_diag_deviations)

        output_file(out_prefix + approach + ".html")

        def bar_plot(data, name, num_buckets=20, print_data=False):
            plot = figure(title=name,
                    x_axis_label='values',
                    y_axis_label='relative amount'
                )

            total_amount_1 = len(data[0])
            total_amount_2 = len(data[1])
            min_val = 10000
            max_val = 0
            for x in data[0]:
                if x is None:
                    return plot
                if x < min_val:
                    min_val = x
                if x > max_val:
                    max_val = x
            for x in data[1]:
                if x is None:
                    return plot
                if x < min_val:
                    min_val = x
                if x > max_val:
                    max_val = x
            bucket_width = float(max_val - min_val) / num_buckets
            #print(max_val, min_val, bucket_width)
            buckets_1 = []
            buckets_2 = []
            buckets_x = []
            for index in range(num_buckets):
                buckets_1.append(0.0)
                buckets_2.append(0.0)
                buckets_x.append(bucket_width * index + min_val + bucket_width/2)
            for x in data[0]:
                buckets_1[int( float(num_buckets*(x - min_val)) / ( (max_val+0.01)-min_val))] += 1.0 / total_amount_1
            for x in data[1]:
                buckets_2[int( float(num_buckets*(x - min_val)) / ((max_val+0.01)-min_val))] += 1.0 / total_amount_2
            if print_data:
                print(buckets_1)
                print(buckets_2)
            plot.vbar(x=buckets_x, width=bucket_width, bottom=0, top=buckets_1, color="blue", legend="accurate", fill_alpha=0.5)
            plot.vbar(x=buckets_x, width=bucket_width, bottom=0, top=buckets_2, color="red", legend="inaccurate", fill_alpha=0.5)
            return plot

        def true_pos_true_neg(data, name, amount=100):
            plot = figure(title=name,
                    y_axis_label='true positive rate',
                    x_axis_label='false positive rate'
                )

            total_amount_1 = len(data[0])
            total_amount_2 = len(data[1])
            min_val = 1
            max_val = 0
            for x in data[0]:
                if x is None:
                    return plot
                if x < min_val:
                    min_val = x
                if x > max_val:
                    max_val = x
            for x in data[1]:
                if x is None:
                    return plot
                if x < min_val:
                    min_val = x
                if x > max_val:
                    max_val = x
            adjust = max_val-min_val
            max_val += adjust*0.1
            min_val -= adjust*0.1

            values = []
            for i in range(amount):
                values.append(min_val + i*adjust/float(amount))
            line_x = []
            line_y = []
            for threshold in values:
                true_pos = 0
                false_pos = 0
                for ele in data[0]:
                    if ele > threshold:
                        true_pos += 1
                for ele in data[1]:
                    if ele > threshold:
                        false_pos += 1
                line_x.append(false_pos / total_amount_2)
                line_y.append(true_pos / total_amount_1)
            plot.line(line_x, line_y)
            #print(min_val, max_val, line_x, line_y)
            return plot

        save(column([
            true_pos_true_neg(mapping_qual, "mapping quality"),
            bar_plot(score_dif, "first and second score diff", 50),
            bar_plot(strip_index, "index of Strip of Consideration", 50),
            bar_plot(seed_coverage, "percentage of query covered by seeds in strip", 50),
            bar_plot(num_seeds_tot, "number of seeds total", 50),
            bar_plot(num_seeds_strip, "number of seeds in strip", 25),
            bar_plot(anc_size, "length of anchor seed", 50),
            bar_plot(anc_ambiguity, "ambiguity of anchor seed", 5),
            bar_plot(max_diag_deviations, "required nmw strip size to reach maximum possible score", 50),
            bar_plot(max_diag_deviations_percent, "required nmw strip size to reach maximum possible score", 50, False),
            bar_plot(max_nmw_areas, "maximum area needleman wunsch", 50),
            bar_plot(nmw_areas, "maximum area needleman wunsch", 50),
            ]))


def analyse_all_approaches_depre(out, db_name, query_size = 100, indel_size = 10):
    output_file(out)
    approaches = getApproachesWithData(db_name)
    plots = [ [], [], [], [], [], [], [], [] ]
    mapping_qual = []
    mapping_qual_illumina = []

    for approach_ in approaches:
        approach = approach_[0]
        results = getResults(db_name, approach, query_size, indel_size)
        hits = {}
        tries = {}
        run_times = {}
        scores = {}
        scores_accurate = {}
        seed_coverage_loss = []
        opt_scores = {}
        opt_score_loss = {}
        nums_seeds = {}
        nums_seeds_chosen = {}
        nucs_by_seed = {}
        mapping_qual.append( ([],[]) )
        mapping_qual_illumina.append( ([],[]) )
        nmw_total = 0

        max_indel = 2*query_size/indel_size
        max_mut = query_size

        sub_illumina = 0.15
        indel_illumina = 0.05
        better_than_optima_count = 0

        def init(d, x, y):
            if x not in d:
                d[x] = {}
            if y not in d[x]:
                d[x][y] = 0
            return d

        for score, score2, optimal_score, result_start, result_end, original_start, num_mutation, num_indels, num_seeds, num_seed_chosen_strip, seed_coverage_chosen_strip, seed_coverage_alignment, mapping_quality, nmw_area, run_time in results:
            if not nmw_area is None:
                nmw_total += nmw_area
            hits = init(hits, num_mutation, num_indels)
            tries = init(tries, num_mutation, num_indels)
            run_times = init(run_times, num_mutation, num_indels)
            if not score is None:
                scores = init(scores, num_mutation, num_indels)
                scores_accurate = init(scores_accurate, num_mutation, num_indels)
            if not optimal_score is None:
                opt_scores = init(opt_scores, num_mutation, num_indels)
            opt_score_loss = init(opt_score_loss, num_mutation, num_indels)
            nums_seeds = init(nums_seeds, num_mutation, num_indels)
            nums_seeds_chosen = init(nums_seeds_chosen, num_mutation, num_indels)
            nucs_by_seed = init(nucs_by_seed, num_mutation, num_indels)

            if near(result_start, original_start, result_end, original_start+query_size):
                hits[num_mutation][num_indels] += 1
                if not optimal_score is None and optimal_score > 0:
                    if optimal_score < score:
                        better_than_optima_count += 1
                        opt_scores[num_mutation][num_indels] += score
                        opt_score_loss[num_mutation][num_indels] += optimal_score/score
                    else:
                        opt_scores[num_mutation][num_indels] += optimal_score
                        opt_score_loss[num_mutation][num_indels] += 1.0
                mapping_qual[-1][0].append(mapping_quality)
                if num_mutation/query_size < sub_illumina and num_indels/query_size < indel_illumina:
                    mapping_qual_illumina[-1][0].append(mapping_quality)

                if seed_coverage_chosen_strip == 0:
                    seed_coverage_loss.append(0)
                else:
                    seed_coverage_loss.append(seed_coverage_alignment / seed_coverage_chosen_strip)

                if not score is None and score > 0:
                    scores_accurate[num_mutation][num_indels] += score
            else:
                mapping_qual[-1][1].append(mapping_quality)
                if num_mutation/query_size < sub_illumina and num_indels/query_size < indel_illumina:
                    mapping_qual_illumina[-1][1].append(mapping_quality)
            tries[num_mutation][num_indels] += 1
            run_times[num_mutation][num_indels] += run_time
            if not num_seeds is None:
                nums_seeds[num_mutation][num_indels] += num_seeds
            if not score is None:
                scores[num_mutation][num_indels] += score
            if not num_seed_chosen_strip is None:
                nums_seeds_chosen[num_mutation][num_indels] += num_seed_chosen_strip

        if better_than_optima_count > 0:
            print("WARNING: aligner got better than optimal score",
                  better_than_optima_count, "times"
                 )

        def makePicFromDict(d, w, h, divideBy, title, ignore_max_n = 0, log = False, set_max = None, set_min=None):
            pic = []
            min_ = 10000.0
            max_ = 0.0
            if len(d.keys()) == 0:
                return None
            else:
                w_keys = sorted(d.keys())
                h_keys = sorted(d[0].keys())

                for x in w_keys:
                    pic.append( [] )
                    for y in h_keys:
                        if x not in d or y not in d[x] or divideBy[x][y] == 0:
                            pic[-1].append( float("nan") )
                        else:
                            pic[-1].append( d[x][y] / divideBy[x][y] )
                            if pic[-1][-1] < min_:
                                min_ = pic[-1][-1]
                            if pic[-1][-1] > max_:
                                max_ = pic[-1][-1]

                for _ in range(ignore_max_n):
                    max_x = 0
                    max_y = 0
                    for x, row in enumerate(pic):
                        for y, p in enumerate(row):
                            if p > pic[max_x][max_y]:
                                max_x = x
                                max_y = y
                    pic[max_x][max_y] = float('nan')
                if ignore_max_n > 0:
                    max_ = 0
                    for row in pic:
                        for p in row:
                            if p > max_:
                                max_ = p

            if set_max is not None:
                max_ = set_max
            if set_min is not None:
                min_ = set_min

            color_mapper = LinearColorMapper(
                    palette=heatmap_palette(light_spec_approximation, 127),
                    low=min_,
                    high=max_
                )
            if log:
                color_mapper = LogColorMapper(
                        palette=heatmap_palette(light_spec_approximation, 127),
                        low=min_,
                        high=max_
                    )

            tick_formater = FuncTickFormatter(code="""
                return Math.max(Math.floor( (tick+1)/2),0) + '; ' +
                        Math.max(Math.floor( (tick)/2),0)"""
                )
            #tick_formater = FuncTickFormatter(code="return 'a')

            plot = figure(title=title,
                    x_range=(0,h), y_range=(0,w),
                    x_axis_label='num ' + str(indel_size) + ' nt insertions; num ' + str(indel_size) + ' nt deletions', y_axis_label='num mutations'
                )
            plot.xaxis.formatter = tick_formater
            plot.image(image=[pic], color_mapper=color_mapper,
                    dh=[w], dw=[h], x=[0], y=[0])

            color_bar = ColorBar(color_mapper=color_mapper, border_line_color=None, location=(0,0))

            plot.add_layout(color_bar, 'left')

            return plot

        total_score = 0
        for values in scores_accurate.values():
            for value in values.values():
                total_score += value
        total_opt_scores = 0
        for values in opt_scores.values():
            for value in values.values():
                total_opt_scores += value

        if total_opt_scores > 0:
            print(approach, ":\ttotalscore:", total_score, "optimal total score:", total_opt_scores, "lost:", 100-100*total_score/total_opt_scores, "%")
        if len(seed_coverage_loss) > 0:
            print(approach, ":\tseed coverage loss:", 
            100-100*sum(seed_coverage_loss)/len(seed_coverage_loss), "%")
        print(approach, ":\ttotal nmw area:", nmw_total)
        def dict_sum(d):
            total = 0
            for x in d.values():
                for y in x.values():
                    total += y
            return total
        print(approach, ":\taverage accuracy:", 100*dict_sum(hits)/dict_sum(tries), "%")

        avg_hits = makePicFromDict(hits, max_mut, max_indel, tries, "accuracy " + approach, set_max=1, set_min=0)
        avg_runtime = makePicFromDict(run_times, max_mut, max_indel, tries, "runtime " + approach, 0)
        avg_score = makePicFromDict(scores, max_mut, max_indel, tries, "score " + approach)
        avg_opt_score = makePicFromDict(opt_score_loss, max_mut, max_indel, tries, "optimal scores " + approach)
        avg_seeds = makePicFromDict(nums_seeds, max_mut, max_indel, tries, "num seeds " + approach)
        avg_seeds_ch = makePicFromDict(nums_seeds_chosen, max_mut, max_indel, tries, "num seeds in chosen SOC " + approach)
        seed_relevance = makePicFromDict(hits, max_mut, max_indel, nums_seeds, "seed relevance " + approach, set_max=0.01, set_min=0)

        if not avg_hits is None:
            plots[0].append(avg_hits)
        if not avg_runtime is None:
            plots[1].append(avg_runtime)
        if not avg_score is None:
            plots[2].append(avg_score)
        if not avg_seeds is None:
            plots[3].append(avg_seeds)
        if not avg_seeds_ch is None:
            plots[5].append(avg_seeds_ch)
        if not seed_relevance is None:
            plots[4].append(seed_relevance)
        if not avg_opt_score is None:
            plots[6].append(avg_opt_score)
        
    
    plot = figure(title="BWA-pic",
        x_axis_label='#wrong / #mapped', y_axis_label='#mapped / total',
        x_axis_type="log", y_range=(0,0.4),
        plot_width=650, plot_height=500,
        min_border_bottom=10, min_border_top=10,
        min_border_left=10, min_border_right=15
    )

    c_palette = heatmap_palette(light_spec_approximation, len(approaches))

    for index, approach_, in enumerate(approaches):
        approach = approach_[0]
        data = mapping_qual[index]

        total_amount_1 = len(data[0])
        total_amount_2 = len(data[1])
        all_data = []
        min_val = 1
        max_val = 0
        for x in data[0]:
            if x is None:
                continue
            all_data.append(x)
            if x < min_val:
                min_val = x
            if x > max_val:
                max_val = x
        for x in data[1]:
            if x is None:
                continue
            all_data.append(x)
            if x < min_val:
                min_val = x
            if x > max_val:
                max_val = x
                max_val = x

        values = []
        amount = 1000
        step = len(all_data) / amount
        c = 0
        for v in sorted(all_data):
            if c >= step:
                c = 0
                values.append(v)
            c += 1

        line_x = []
        line_y = []
        for val in values:
            mapped = 0
            wrong = 0
            total = total_amount_1 + total_amount_2
            for ele in data[0]:
                if not ele is None and ele > val:
                    mapped += 1
            for ele in data[1]:
                if not ele is None and ele > val:
                    mapped += 1
                    wrong += 1

            if mapped > 0:
                line_x.append( wrong/mapped )
                line_y.append( mapped/total )

        plot.line(line_x, line_y, legend=approach, color=c_palette[index], line_width=2)
        plot.x(line_x, line_y, legend=approach, color=c_palette[index], size=6)

    plot.legend.location = "top_left"
    plots[-1].append(plot)

    save(layout(plots))

def analyse_all_approaches(out, db_name, query_size = 100, indel_size = 10):
    output_file(out)
    approaches = getApproachesWithData(db_name)
    plots = [ [], [], [] ]
    mapping_qual = []
    yax = True
    for approach_ in approaches:
        approach = approach_[0]
        results = getResults(db_name, approach, query_size, indel_size)
        hits = {}
        tries = {}
        run_times = {}
        scores = {}
        nums_seeds = {}
        mapping_qual.append( ([],[]) )

        max_indel = 0
        if indel_size > 0:
            max_indel = 2*query_size/indel_size
        max_mut = query_size

        def init(d, x, y):
            if x not in d:
                d[x] = {}
            if y not in d[x]:
                d[x][y] = 0
            return d

        for score, score2, optimal_score, result_start, result_end, original_start, num_mutation, num_indels, num_seeds, num_seeds_chosen_strip, seed_coverage_chosen_strip, seed_coverage_alignment, mapping_quality, nmw_area, run_time in results:
            hits = init(hits, num_mutation, num_indels)
            tries = init(tries, num_mutation, num_indels)
            run_times = init(run_times, num_mutation, num_indels)
            if not score is None:
                scores = init(scores, num_mutation, num_indels)
            nums_seeds = init(nums_seeds, num_mutation, num_indels)

            #print(result_start, original_start)
            if near(result_start, original_start, result_end, original_start+query_size):
                hits[num_mutation][num_indels] += 1
                mapping_qual[-1][0].append(mapping_quality)
            else:
                mapping_qual[-1][1].append(mapping_quality)
            tries[num_mutation][num_indels] += 1
            run_times[num_mutation][num_indels] += run_time
            if not num_seeds is None:
                nums_seeds[num_mutation][num_indels] += num_seeds
            if not score is None:
                scores[num_mutation][num_indels] += score


        def makePicFromDict(d, w, h, divideBy, title, ignore_max_n = 0, log = False, set_max = None, set_min=None, xax=True, yax=True):
            pic = []
            min_ = 10000.0
            max_ = 0.0
            if len(d.keys()) == 0:
                return None

            for x in range(0,401,20):
                pic.append( [] )
                for y in range(0,20,2):
                    if x not in d or y not in d[x] or divideBy[x][y] == 0:
                        pic[-1].append( float("nan") )
                    else:
                        pic[-1].append( d[x][y] / divideBy[x][y] )
                        if pic[-1][-1] < min_:
                            min_ = pic[-1][-1]
                        if pic[-1][-1] > max_:
                            max_ = pic[-1][-1]

            for _ in range(ignore_max_n):
                max_x = 0
                max_y = 0
                for x, row in enumerate(pic):
                    for y, p in enumerate(row):
                        if p > pic[max_x][max_y]:
                            max_x = x
                            max_y = y
                pic[max_x][max_y] = float('nan')
            if ignore_max_n > 0:
                max_ = 0
                for row in pic:
                    for p in row:
                        if p > max_:
                            max_ = p

            if set_max is not None:
                max_ = set_max
            if set_min is not None:
                min_ = set_min

            color_mapper = LinearColorMapper(
                    palette=heatmap_palette(light_spec_approximation, 256),
                    low=min_,
                    high=max_
                )
            if log:
                color_mapper = LogColorMapper(
                        palette=heatmap_palette(light_spec_approximation, 256),
                        low=min_,
                        high=max_
                    )

            plot_width=500
            if yax:
                plot_width += 150
            plot_height=500
            if xax:
                plot_height += 40
            plot = figure(title=title,
                    x_range=(0,h), y_range=(0,w),
                    x_axis_label=str(indel_size) + 'nt indels', y_axis_label='mutations',
                    plot_width=plot_width, plot_height=plot_height,
                    min_border_bottom=10, min_border_top=10,
                    min_border_left=10, min_border_right=15,
                    tools=["save"]
                )
            #plot.xaxis.formatter = tick_formater
            plot.image(image=[pic], color_mapper=color_mapper,
                    dh=[w], dw=[h], x=[0], y=[0])

            font = "Helvetica"
            font_size = '15pt'
            if yax:
                color_bar = ColorBar(color_mapper=color_mapper, border_line_color=None, location=(0,0))
                color_bar.major_label_text_font=font
                color_bar.major_label_text_font_size=font_size
                plot.add_layout(color_bar, 'left')

            #if not xax:
            #    plot.xaxis.visible = False
            #if not yax:
            #    plot.yaxis.visible = False
            if not title is None:
                plot.title.text_font=font
                plot.title.text_font_size=font_size
            plot.legend.label_text_font=font
            plot.legend.label_text_font_size=font_size
            plot.axis.axis_label_text_font=font
            plot.axis.major_label_text_font=font
            plot.axis.axis_label_text_font_size=font_size
            plot.axis.major_label_text_font_size=font_size

            return plot
        if indel_size > 0:
            avg_hits = makePicFromDict(hits, 400, max_indel, tries, approach, set_max=1, set_min=0, yax=False)
            avg_runtime = makePicFromDict(run_times, 400, max_indel, tries, None, 0, False, 0.1, 0, yax=False)
            #avg_score = makePicFromDict(scores, max_mut, max_indel, tries, "score " + approach, yax=yax)
            #avg_seeds = makePicFromDict(nums_seeds, 400, max_indel, tries, approach, yax=yax)
            #avg_rel = makePicFromDict(hits, 400, max_indel, nums_seeds, approach, yax=yax, set_min=0, set_max=0.01)
            yax = False

            if not avg_hits is None:
                plots[0].append(avg_hits)
            if not avg_runtime is None:
                plots[1].append(avg_runtime)
            #if not avg_seeds is None:
            #    plots[2].append(avg_seeds)
            #if not avg_rel is None:
            #    plots[3].append(avg_rel)

    plot = figure(title="BWA-pic",
            x_axis_label='#wrong / #mapped', y_axis_label='#mapped / total',
            x_axis_type="log", y_range=(0,0.4),
            plot_width=650, plot_height=500,
            min_border_bottom=10, min_border_top=10,
            min_border_left=10, min_border_right=15
        )

    c_palette = heatmap_palette(light_spec_approximation, len(approaches))

    for index, approach_, in enumerate(approaches):
        approach = approach_[0]
        data = mapping_qual[index]

        total_amount_1 = len(data[0])
        total_amount_2 = len(data[1])
        all_data = []
        min_val = 1
        max_val = 0
        for x in data[0]:
            if x is None:
                continue
            all_data.append(x)
            if x < min_val:
                min_val = x
            if x > max_val:
                max_val = x
        for x in data[1]:
            if x is None:
                continue
            all_data.append(x)
            if x < min_val:
                min_val = x
            if x > max_val:
                max_val = x

        values = []
        amount = 1000
        step = len(all_data) / amount
        c = 0
        for v in sorted(all_data):
            if c >= step:
                c = 0
                values.append(v)
            c += 1

        line_x = []
        line_y = []
        for val in values:
            mapped = 0
            wrong = 0
            total = total_amount_1 + total_amount_2
            for ele in data[0]:
                if not ele is None and ele > val:
                    mapped += 1
            for ele in data[1]:
                if not ele is None and ele > val:
                    mapped += 1
                    wrong += 1

            if mapped > 0:
                line_x.append( wrong/mapped )
                line_y.append( mapped/total )

        plot.line(line_x, line_y, legend=approach, color=c_palette[index])
        plot.x(line_x, line_y, legend=approach, color=c_palette[index])

    plot.legend.location = "top_left"
    plots[-1].append(plot)

    save(gridplot(plots))

def compare_approaches(out, approaches, db_name, query_size = 100, indel_size = 10):
    output_file(out)
    plots = []

    max_indel = 2*query_size/indel_size
    max_mut = query_size

    def get_data(db_name, approach, query_size, indel_size):
        results = getResults(db_name, approach, query_size, indel_size, human_genome)
        hits = {}
        tries = {}
        scores = {}

        def init(d, x, y):
            if x not in d:
                d[x] = {}
            if y not in d[x]:
                d[x][y] = 0
            return d

        for score, result_start, result_end, original_start, num_mutation, num_indels in results:
            hits = init(hits, num_mutation, num_indels)
            tries = init(tries, num_mutation, num_indels)
            if not score is None:
                scores = init(scores, num_mutation, num_indels)
            if near(result_start, original_start, result_end, original_start+query_size):
                hits[num_mutation][num_indels] += 1
            tries[num_mutation][num_indels] += 1
            if not score is None:
                scores[num_mutation][num_indels] += score

        return (hits, tries, scores)

    def convertToList(d, w, h, divideBy):
        pic = []
        if len(d.keys()) == 0:
            return None
        else:
            w_keys = sorted(d.keys())
            h_keys = sorted(d[0].keys())
            for x in w_keys:
                pic.append( [] )
                for y in h_keys:
                    if x not in d or y not in d[x]:
                        pic[-1].append( float("nan") )
                    else:
                        pic[-1].append( d[x][y] / divideBy[x][y] )
        return pic

    def sub(arr1, arr2):
        if arr1 is None or arr2 is None:
            return None
        for x, row in enumerate(arr2):
            for y, ele in enumerate(row):
                arr1[x][y] -= ele
        return arr1

    l_1 = get_data(db_name, approaches[0], query_size, indel_size)
    l_2 = get_data(db_name, approaches[1], query_size, indel_size)

    hits = sub(convertToList(l_1[0], max_indel, max_mut, l_1[1]),
            convertToList(l_2[0], max_indel, max_mut, l_2[1]))

    scores = sub(convertToList(l_1[2], max_indel, max_mut, l_1[1]),
            convertToList(l_2[2], max_indel, max_mut, l_2[1]))


    def makePic(pic, w, h, title, log = False):
        if pic is None:
            return None
        def rgb(minimum, maximum, value):
            def clamp(x):
                return max(0, min(x, 255))
            value = min(max(minimum, value), maximum)
            red = green = blue = 0.0
            if value < 0:
                red = value/minimum
            else:
                green = value/maximum
            return "#{0:02x}{1:02x}{2:02x}".format(clamp(int(red * 255)), clamp(int(green * 255)),
                                                clamp(int(blue * 255)))

        def heatmap_palette(num_colors):
            #Color palette for visualization.
            return [rgb(-1, 1, x) for x in np.linspace(-1, 1, num_colors)]

        max_ = -100000
        min_ =  100000

        for row in pic:
            for ele in row:
                if ele > max_:
                    max_ = ele
                if ele < min_:
                    min_ = ele

        if min_ < 0:
            min_ = -min_
        range_ = max_
        if min_ > range_:
            range_ = min_

        color_mapper = LinearColorMapper(palette=heatmap_palette(255), low=-range_, high=range_)
        if log:
            color_mapper = LogColorMapper(palette=heatmap_palette(255), low=-range_, high=range_)

        tick_formater = FuncTickFormatter(code="return Math.max(Math.floor( (tick+1)/2),0) + '; ' + Math.max(Math.floor( (tick)/2),0)")
        #tick_formater = FuncTickFormatter(code="return 'a')

        plot = figure(title=title,
                x_range=(0,h), y_range=(0,w),
                x_axis_label='num ' + str(indel_size) + ' nt insertions; num ' + str(indel_size) + ' nt deletions', y_axis_label='num mutations'
            )
        plot.xaxis.formatter = tick_formater
        plot.image(image=[pic], color_mapper=color_mapper,
                dh=[w], dw=[h], x=[0], y=[0])

        color_bar = ColorBar(color_mapper=color_mapper, border_line_color=None, location=(0,0))

        plot.add_layout(color_bar, 'left')


        return plot

    desc = approaches[0] + " - " + approaches[1]

    avg_hits = makePic(hits, max_mut, max_indel, "accuracy " + desc)
    avg_score = makePic(scores, max_mut, max_indel, "score " + desc)

    if not avg_hits is None:
        plots.append(avg_hits)
    if not avg_score is None:
        plots.append(avg_score)

    save(row(plots))


def get_ambiguity_distribution(reference, min_len=10, max_len=20):
    def get_all_queries(l):
        if l <= 0:
            yield ""
        else:
            for q in get_all_queries(l-1):
                yield q + "A"
                yield q + "C"
                yield q + "G"
                yield q + "T"
    def get_random_queries(l, amount):
        for _ in range(amount):
            q = ""
            for _ in range(l):
                char = random.randint(1,4)
                if char == 1:
                    q += "A"
                elif char == 2:
                    q += "C"
                elif char == 3:
                    q += "G"
                elif char == 4:
                    q += "T"
            yield q
    fm_index = FMIndex()
    fm_index.load(reference)
    num_queries = 100000

    r1max = 10
    r2size = 15

    indices1 = []
    indices2 = []
    for i in range(r1max):
        indices1.append(i)
    for i in range(r2size):
        indices2.append(2**i+r1max)
    data1 = []
    data2 = []
    for l in range(min_len, max_len):
        data1.append( [] )
        for _ in range(r1max):
            data1[-1].append(0.0)
        data2.append( [] )
        for _ in range(r2size):
            data2[-1].append(0.0)
        for q in get_random_queries(l, num_queries):
            ambiguity = fm_index.get_ambiguity(NucSeq(q))
            if ambiguity < r1max:
                data1[-1][int(ambiguity)] += 1.0/num_queries
            elif int(math.log2(ambiguity - r1max+1)) < r2size:
                data2[-1][int(math.log2(ambiguity - r1max+1))] += 1.0/num_queries

    print(indices1)
    print(data1)
    print(indices2)
    print(data2)

    color_mapper = LinearColorMapper(
                    palette=heatmap_palette(light_spec_approximation, 256),
                    low=0,
                    high=1
                )

    plot = figure(title="ambiguity on human genome",
            x_range=(0,r1max), y_range=(min_len, max_len),
            x_axis_label='ambiguity', y_axis_label='sequence length',
            plot_width=700, plot_height=500,
            min_border_bottom=10, min_border_top=10,
            min_border_left=10, min_border_right=15,
            tools=["save"]
        )
    plot.image(image=[data1], color_mapper=color_mapper,
            dh=[max_len - min_len], dw=[r1max], x=[0], y=[min_len])

    plot2 = figure(x_range=(r1max,2**r2size+r1max), y_range=(min_len, max_len),
            min_border_bottom=10, min_border_top=10,
            min_border_left=20, min_border_right=15,
            plot_width=500, plot_height=500,tools=[],
            x_axis_type="log"
        )
    plot2.image(image=[data2], color_mapper=color_mapper,
            dh=[max_len - min_len], dw=[2**r2size+r1max], x=[r1max], y=[min_len])

    font = "Helvetica"
    font_size = '15pt'
    color_bar = ColorBar(color_mapper=color_mapper, border_line_color=None, location=(0,0))
    color_bar.major_label_text_font=font
    color_bar.major_label_text_font_size=font_size
    plot.add_layout(color_bar, 'left')

    plot.legend.label_text_font=font
    plot.legend.label_text_font_size=font_size
    plot.axis.axis_label_text_font=font
    plot.axis.major_label_text_font=font
    plot.axis.axis_label_text_font_size=font_size
    plot.axis.major_label_text_font_size=font_size

    plot2.legend.label_text_font=font
    plot2.legend.label_text_font_size=font_size
    plot2.yaxis.visible = False
    plot2.axis.axis_label_text_font=font
    plot2.axis.major_label_text_font=font
    plot2.axis.axis_label_text_font_size=font_size
    plot2.axis.major_label_text_font_size=font_size

    show(gridplot( [[plot, plot2]] ))

"""
int iGap = 20;
int iExtend = 1;
int iMatch = 10;
int iMissMatch = 4;

MA:
AATC--AGAT--ATTCTT        reference
IIII  || |     III
AATCCCAG-TGG---CTT        query

libMA.debugNW(
    NucSeq("AGATATT"),
    NucSeq("CCAGTGG")
)
exit()
smw = SMW(True)
smw.print = True

smw.execute(
    NucSeq("AGATATT"),
    NucSeq("CCAGTGG")
)
exit()
"""




#create_as_sequencer_reads("/mnt/ssd1/illumina.db", 1000)
#test_my_approaches("/mnt/ssd1/illumina.db")
#analyse_all_approaches("illumina.html","/mnt/ssd1/illumina.db", 150, 0)

#high quality picture

#l = 200
#il = 10
#createSampleQueries(human_genome, "/mnt/ssd1/test.db", l, il, 32, high_qual=False, smaller_box=True)
#test_my_approaches("/mnt/ssd1/test.db")
#analyse_all_approaches("test.html","/mnt/ssd1/test.db", l, il)
#compare_approaches("comp.html", ["BWA-MEM", "MA 1"],"/mnt/ssd1/test.db", l, il)
#compare_approaches("comp2.html", ["BWA-MEM", "MA 2"],"/mnt/ssd1/test.db", l, il)
#analyse_all_approaches_depre("test_depre.html","/mnt/ssd1/test.db", l, il)
#analyse_detailed("stats/", "/mnt/ssd1/test.db")
#exit()

amount = 2**10
#createSampleQueries(human_genome, "/mnt/ssd1/default.db", 1000, 100, amount)
#createSampleQueries(human_genome, "/mnt/ssd1/long.db", 30000, 100, amount)
#createSampleQueries(human_genome, "/mnt/ssd1/short.db", 250, 25, amount)
#createSampleQueries(human_genome, "/mnt/ssd1/shortIndels.db", 1000, 50, amount)
#createSampleQueries(human_genome, "/mnt/ssd1/longIndels.db", 1000, 200, amount)
#createSampleQueries(human_genome, "/mnt/ssd1/insertionOnly.db", 1000, 100, amount, in_to_del_ratio=1)
#createSampleQueries(human_genome, "/mnt/ssd1/deletionOnly.db", 1000, 100, amount, in_to_del_ratio=0)
#createSampleQueries(human_genome, "/mnt/ssd1/zoomLine.db", 1000, 100, amount, high_qual=True, only_first_row=True)
#createSampleQueries(human_genome, "/mnt/ssd1/zoomSquare.db", 1000, 100, amount, high_qual=True, smaller_box=True)

import measure_time

measure_time.test_all()

#test_my_approaches("/mnt/ssd1/default.db")
#test_my_approaches("/mnt/ssd1/short.db")
#test_my_approaches("/mnt/ssd1/shortIndels.db")
#test_my_approaches("/mnt/ssd1/longIndels.db")
#test_my_approaches("/mnt/ssd1/insertionOnly.db")
#test_my_approaches("/mnt/ssd1/deletionOnly.db")

analyse_all_approaches("default.html","/mnt/ssd1/default.db", 1000, 100)
#analyse_all_approaches("long.html","/mnt/ssd1/long.db", 30000, 100)
analyse_all_approaches("short.html","/mnt/ssd1/short.db", 250, 25)
analyse_all_approaches("shortIndels.html","/mnt/ssd1/shortIndels.db", 1000, 50)
analyse_all_approaches("longIndels.html","/mnt/ssd1/longIndels.db", 1000, 100)
analyse_all_approaches("insertionOnly.html","/mnt/ssd1/insertionOnly.db", 1000, 100)
analyse_all_approaches("deletionOnly.html","/mnt/ssd1/deletionOnly.db", 1000, 100)
analyse_all_approaches("zoomLine.html","/mnt/ssd1/zoomLine.db", 1000, 100)
analyse_all_approaches("zoomSquare.html","/mnt/ssd1/zoomSquare.db", 1000, 100)
