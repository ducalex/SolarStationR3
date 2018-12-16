class AVGr
{
  private:
  float m_currentAvg;
  int m_count;
  int m_maximum;
  
  public:
  AVGr(int maximum) 
  {
    m_currentAvg = 0.0;
    
    m_maximum = maximum;
    m_count = 0;
  }

  void add(float newValue) {
    if (m_count + 1 < m_maximum)
      m_count++;      
    m_currentAvg = (m_currentAvg * (((float)m_count - 1) / (float)m_count)) + newValue / (float)m_count;
  }

  float getAvg() {
    return m_currentAvg;
  }
};
