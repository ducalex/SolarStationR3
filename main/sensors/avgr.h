class AVGr
{
  private:
    short m_nsamples;
    short m_count;
    float m_min; // All time min, not min in last nsamples
    float m_max; // All time max, not max in last nsamples
    float m_avg; // Decaying weighted average faking avg of last nsamples
    float m_val; // Last added value

  public:
    AVGr(short maximum)
    {
        m_min = m_max = m_avg = m_val = 0.00f;
        m_nsamples = maximum;
        m_count = 0;
    }

    void add(float newValue)
    {
        if (m_count + 1 < m_nsamples)
            m_count++;
        m_avg = (m_avg * ((float)(m_count - 1) / m_count)) + newValue / m_count;
        m_val = newValue;
        if (newValue < m_min || m_count == 1) m_min = newValue;
        if (newValue > m_max || m_count == 1) m_max = newValue;
    }

    float getAvg()
    {
        return m_avg;
    }

    float getMin()
    {
        return m_min;
    }

    float getMax()
    {
        return m_max;
    }

    float getVal()
    {
        return m_val;
    }
};
