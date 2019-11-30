class Renderer():
    def __init__(self, plot, l_plot, d_plot, xs, xe, ys, ye, pack, fm_index, sv_db, run_id, ground_truth_id, min_score,
                 max_num_ele, dataset_name, active_tools, checkbox_group, read_plot, selected_read_id, l_read_plot,
                 d_read_plot, index_prefix):
        self.plot = plot
        self.l_plot = l_plot
        self.d_plot = d_plot
        self.xs = xs
        self.xe = xe
        self.ys = ys
        self.ye = ye
        self.pack = pack
        self.fm_index = fm_index
        self.sv_db = sv_db
        self.run_id = run_id
        self.ground_truth_id = ground_truth_id
        self.min_score = min_score
        self.max_num_ele = max_num_ele
        self.dataset_name = dataset_name
        self.active_tools = active_tools
        self.checkbox_group = checkbox_group
        self.read_plot = read_plot
        self.selected_read_id = selected_read_id
        self.index_prefix = index_prefix
        self.w = None
        self.h = None
        self.params = None
        self.l_read_plot = l_read_plot
        self.d_read_plot = d_read_plot
        self.quads = []
        self.read_ids = set()
        self.give_up_factor = 1000

    # imported methdos
    from ._render import render
    from ._render_overview import render_overview
    from ._render_calls import render_calls
    from ._render_jumps import render_jumps
    from ._render_reads import render_reads
    from ._render_nucs import render_nucs